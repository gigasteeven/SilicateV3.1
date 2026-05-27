#include "updater.hpp"

#include <Zydis/Zydis.h>

#include <Geode/Geode.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <safetyhook.hpp>

#include "Geode/cocos/CCScheduler.h"
#include "assist/autoclicker.hpp"
#include "assist/hitboxes.hpp"
#include "bot.hpp"
#include "checkpoint/fix.hpp"
#include "label/label.hpp"
#include "render/renderer.hpp"
#include "replay/system.hpp"
#include "scheduler.hpp"
#include "speedhack.hpp"
#include "trajectory/trajectory.hpp"
#include "util/midhook.hpp"
#include "util/profile.hpp"

using namespace geode::prelude;

float BotUpdater::getTimeWarp() {
    if (auto pl = PlayLayer::get(); pl) {
        return pl->m_gameState.m_timeWarp;
    }

    return 1.0;
}

/**
 * Calculate the number of steps to take based on the delta time.
 * This also handles overflow from previous frames and manages
 * rendering behavior.
 */
void BotUpdater::calculateSteps(float dt, float targetDt) {
    dt += m_tpsOverflow;

    float wantedDt = targetDt * fminf(getTimeWarp(), 1.0);
    if (wantedDt == 0.0) {
        return;
    }

    int steps = std::floor(dt / wantedDt);

    int stepLimit = m_maxUPR->inner();

    if (m_respawnTimer > 0) {
        // steps = 0;
        // dt = 0.0f;             // but also don't add overflow
        m_tpsOverflow = 0.0f;  // reset overflow
    }

    // Aka "how many steps can we take in one VISUAL frame"
    if (m_useVisualUpdates->inner()) {
        float fps = GameManager::get()->m_customFPSTarget;
        if (fps <= 10.0) {
            fps = 240.0;
        }
            
        int modifier = (int)m_tps->inner() / (int)fps;

        stepLimit *= std::max(1, modifier);
    }

    // Rendering shouldn't have an UPR limit
    bool rendering = Renderer::get()->isRecording();

    if (!m_realTime->inner() && !rendering) {
        steps = std::min(steps, stepLimit);
    }

    m_tpsOverflow = dt - steps * wantedDt;
    m_shouldRender = false;

    // Again, rendering doesn't have an UPR limit so no need
    // to reset tps overflow here
    if (!m_realTime->inner() && !rendering) {
        if (steps == stepLimit) {
            m_tpsOverflow = 0.0;
        }
    }

    if (isPaused() && !rendering) {
        steps = 1;  // ALWAYS frame advance one step
        m_tpsOverflow = 0;
        m_shouldRender = true;
        if (PlayLayer::get()) {
            PlayLayer::get()->m_extraDelta = 0.0f;  // reset extra delta
        }
    }

    totalStepCount = steps;
    estimatedStepCount = steps;
}

static void runFastLockDeltaUpdates(float realDt,
                                    std::function<void(float)> update) {
    auto bot = Bot::get();
    auto& updater = bot->updater();

    auto& nextInput = Bot::get()->replaySystem().getCurrentQueuedInput();

    SCOPED_TIMER("fastLockDelta")
    updater.m_allowedToProcessActions = false;
    if (!nextInput.has_value()) {
        // run updates normally
        updater.calculateSteps(
            realDt * updater.getTimeWarp() * updater.m_speedhack->inner(),
            updater.getPhysicsDt());

        if (updater.estimatedStepCount >= 1) {
            update(realDt * updater.m_speedhack->inner());
        }
    } else {
        updater.calculateSteps(
            realDt * updater.getTimeWarp() * updater.m_speedhack->inner(),
            updater.getPhysicsDt());

        int steps = updater.totalStepCount;

        while (steps > 0) {
            auto& input = Bot::get()->replaySystem().getCurrentQueuedInput();
            uint64_t safeSteps =
                input.has_value() ? input->m_frame - updater.getFrame() : steps;
            safeSteps = std::min(safeSteps, static_cast<uint64_t>(steps));

            if (safeSteps > 0) {
                updater.estimatedStepCount = safeSteps;
                update(updater.getPhysicsDt() * safeSteps);
                steps -= safeSteps;
            }

            if (steps > 0) {
                updater.estimatedStepCount = 1;
                update(realDt * updater.m_speedhack->inner());
                steps -= 1;
            }
        }
    }
}

static void runSlowLockDeltaUpdates(float realDt,
                                    std::function<void(float)> update,
                                    bool calculateSsb) {
    GJBaseGameLayer* pl = PlayLayer::get();
    if (!pl) {
        pl = LevelEditorLayer::get();
    }

    auto bot = Bot::get();
    auto& updater = bot->updater();

    float newTfx = pl->timeForPos(pl->m_player1->m_position, 0,
                                  pl->m_gameState.m_currentChannel, true, 0);
    float ssbDelta = std::abs(newTfx - updater.m_lastTfp);

    if (!calculateSsb || ssbDelta <= 0) {
        ssbDelta = updater.getPhysicsDt();
    }

    updater.m_lastTfp = newTfx;
    float delta = updater.getPhysicsDt();

    updater.calculateSteps(
        realDt * updater.getTimeWarp() * updater.m_speedhack->inner(),
        ssbDelta);

    SCOPED_TIMER("slowLockDelta")
    updater.m_shouldRender = false;
    for (int i = 0; i < updater.estimatedStepCount - 1; i++) {
        update(delta);
        if (!((PlayLayer*)pl)->m_hasCompletedLevel &&
            updater.estimatedStepCount > 0) {
            if (calculateSsb) {
                float new_tfx =
                    pl->timeForPos(pl->m_player1->m_position, 0,
                                   pl->m_gameState.m_currentChannel, true, 0);

                ssbDelta = std::abs(new_tfx - updater.m_lastTfp);
                if (ssbDelta <= 0) {
                    ssbDelta = updater.getPhysicsDt();
                }
                updater.m_lastTfp = new_tfx;
                updater.calculateSteps(realDt * updater.getTimeWarp() *
                                           updater.m_speedhack->inner(),
                                       ssbDelta);  // yes we have to actively
                                                   // recalculate steps
            }
        }
    }

    updater.m_shouldRender = true;
    if (updater.estimatedStepCount > 0) {
        update(delta);
    }
}

void BotUpdater::runUpdates(std::function<void(float)> update, float realDt,
                            bool frozen) {
    auto bot = Bot::get();
    m_allowedToProcessActions = true;

    // Frozen updates are with dt=0 and shouldn't run any physics updates
    if (frozen) {
        SCOPED_TIMER("frozenGameUpdate")
        m_onlyRefresh = true;
        update(realDt);
        m_onlyRefresh = false;
        if (PlayLayer::get()) {
            bot->trajectory().update(PlayLayer::get());
        }
        return;
    }

    SCOPED_TIMER("fullGameUpdate")

    GJBaseGameLayer* pl = PlayLayer::get();
    bool isPlayLayer = true;
    if (!pl) {
        pl = LevelEditorLayer::get();
        isPlayLayer = false;
    }

    if (!pl || (isPlayLayer && ((PlayLayer*)pl)->m_isPaused)) {
        this->m_tpsOverflow = 0;
        update(realDt);  // be quiet
        return;
    }

    if (auto renderer = Renderer::get();
        renderer->isRecording() && renderer->m_collectAudio) {
        realDt = renderer->getDt();
    }

    if (this->isPaused() && !Renderer::get()->isRecording()) {
        this->m_tpsOverflow = 0;
        m_respawnTimer = 0;
        if (this->consumeStep()) {
            this->m_shouldRender = true;
        } else {
            return;
        }
    }

    bool calculateSsb = false;
    if (isPlayLayer) {
        !((PlayLayer*)pl)->m_isPaused &&
            !((PlayLayer*)pl)->m_hasCompletedLevel && pl->m_started &&
            !pl->m_isPlatformer && m_ssbFix->inner() &&
            Renderer::get()->isRecording();
    }

    bool useAccLockDelta =
        m_lockDelta->inner() &&
        (m_lockDeltaMode->inner() == LockDeltaMode::Accuracy ||
         Renderer::get()->isRecording());

    if ((m_lockDelta->inner() || useAccLockDelta) && isPlayLayer) {
        if (this->useFastLockDelta()) {
            runFastLockDeltaUpdates(realDt, update);
        } else {
            runSlowLockDeltaUpdates(realDt, update, calculateSsb);
        }
    } else {
        m_shouldRender = true;
        if (m_respawnTimer > 0) {
            m_tpsOverflow = 0.0;
            pl->m_extraDelta = 0.0;
            // this->estimatedStepCount = 0;
            // this->totalStepCount = 0;
        }

        update(realDt * m_speedhack->inner());
    }

    bool disableLabels = Renderer::get()->isRecording();

    bot->labels().update(disableLabels);
    bot->trajectory().update(PlayLayer::get());
}

void BotUpdater::breakLoop() {
    this->estimatedStepCount = 0;
    this->totalStepCount = 0;
    this->m_tpsOverflow = 0.0;
}

bool BotUpdater::isLockDelta() { return m_lockDelta->inner(); }

void BotUpdater::setFps(double fps) {
    if (fps <= 0.0) {
        return;
    }

    m_fps = fps;

    CCDirector::sharedDirector()->setAnimationInterval(1. / m_fps);

    auto gm = GameManager::sharedState();
    gm->m_customFPSTarget = m_fps;
    gm->setGameVariable("0116", true);
}

void BotUpdater::setSpeed(double speed) {
    if (speed <= 0.0) {
        return;
    }

    CCDirector::sharedDirector()->setAnimationInterval(1. / m_fps);
    auto gm = GameManager::sharedState();
    gm->m_customFPSTarget = m_fps;
    gm->setGameVariable("0116", true);
}

void BotUpdater::updateAudioSpeedhack() {
    FMOD::ChannelGroup* master;
    FMODAudioEngine::get()->m_system->getMasterChannelGroup(&master);

    if (m_speedhackAudio->inner()) {
        master->setPitch(m_speedhack->inner());
    } else {
        master->setPitch(1.0);
    }
}

void BotUpdater::runFrozenTick() {
    if (m_frozenScheduledFunctions.empty()) {
        return;  // no need to waste resources
    }

    for (auto& func : m_frozenScheduledFunctions) {
        func(0.0f);
    }

    runUpdates(
        [](float dt) {
            cocos2d::CCDirector::sharedDirector()->getScheduler()->update(dt);
        },
        0.0f, true);

    m_frozenScheduledFunctions.clear();
    // CCEGLView::get()->swapBuffers();
}

inline double BotUpdater::getVisualDt() {
    return 1. / (m_fps * m_speedhack->inner());
}

uint32_t BotUpdater::getFrame() {
    // Playing a level, with intentional death
    if (auto pl = PlayLayer::get(); pl) {
        return m_frame + m_frameOnLastAttempt;
    }

    // No intentional death in editor, use built in frame counter
    if (auto lel = LevelEditorLayer::get(); lel) {
        return static_cast<uint32_t>(std::max(0, static_cast<int>(lel->m_gameState.m_currentProgress) - 1));
    }

    return 0;
}

bool BotUpdater::useFastLockDelta() {
    return this->m_lockDelta->inner() &&
           this->m_lockDeltaMode->inner() ==
               BotUpdater::LockDeltaMode::Performance &&
           Bot::get()->isPlaying() && !Renderer::get()->isRecording();
}

void BotUpdater::backwardsStep(int n) {
    m_paused->inner() = true;

    if (n <= 0) {
        return;
    }

    if (m_backwardsStepping->inner()) {
        // this->scheduleFrozenFunction([](float) {
        auto pl = PlayLayer::get();
        auto bot = Bot::get();

        for (int i = 0; i < n - 1; i++) {
            bot->practiceFix().dropLastStoredFrame();
        }

        if (bot->practiceFix().m_storedFrames.size() <= 1) {
            return;
        }

        // bot->replaySystem().m_forceNextInput = true;
        // pl->processQueuedButtons();
        // bot->replaySystem().m_forceNextInput = false;
        //
        bot->practiceFix().m_loadCheckpoint = true;
        bot->practiceFix().m_isBackstep = true;
        pl->resetLevel();
        bot->practiceFix().m_loadCheckpoint = false;

        // for (int i = 0; i < pl->m_objects->count(); i++) {
        //     GameObject* object =
        //         (GameObject*)pl->m_objects->objectAtIndex(i);

        //     CCPoint pos = object->getRealPosition();

        //     object->resetObject();
        //     // if (object->m_outerSectionIndex >= 0) {
        //     //     if (object->m_positionX > 0.0) {

        //     //     }
        //     // }

        //     object->m_isDirty = true;
        //     object->setObjectRectDirty(true);
        //     object->setOrientedRectDirty(true);
        // }

        // bot->practiceFix().restorePreviousFrame();

        // pl->updateCamera(0.0);
        // pl->updateVisibility(0.0);
        // pl->updateGradientLayers();
        // pl->updateShaderLayer(0.0);

        // pl->stopActionByTag(0x10);

        // bot->replaySystem().onReset(bot->updater().getFrame());

        // bot->labels().update(false);

        // if (bot->isRecording()) {
        //     auto& rs = bot->replaySystem();

        //     for (auto& [b, lastInput] : rs.m_lastInputs) {
        //         if (lastInput.m_frame >= bot->updater().getFrame()) {
        //             bool queue = true;

        //             if (lastInput.m_button ==
        //             sv2::Input::InputType::Jump
        //             &&
        //                 pl->m_player1->m_jumpBuffered &&
        //                 lastInput.m_holding && !lastInput.m_player2)
        //                 { queue = false;
        //             }

        //             if (lastInput.m_button ==
        //             sv2::Input::InputType::Jump
        //             &&
        //                 pl->m_player2->m_jumpBuffered &&
        //                 lastInput.m_holding && lastInput.m_player2) {
        //                 queue = false;
        //             }

        //             // queue that shit
        //             if (queue)
        //                 pl->queueButton(
        //                     static_cast<int>(lastInput.m_button),
        //                     lastInput.m_holding,
        //                     lastInput.m_player2);
        //         }
        //     }

        //     rs.m_lastInputs.clear();
        // }

        // bot->updater().m_tpsOverflow = 0.0;
        // });
    }
}

void BotUpdater::findBestFrameCandidate() {
    auto bot = Bot::get();
    auto pl = PlayLayer::get();
    if (!pl) return;
    if (!m_backwardsStepping->inner()) return;
    auto trajectory = bot->trajectory().unsafeInner();
    if (!trajectory) return;

    int bestPred = 0;
    uint32_t bestFrame = 0;
    uint32_t iters = 0;

    this->setPaused(true);

    int lowestAcceptable =
        static_cast<int>((float)trajectory->getPredictionLength() *
                         m_acceptablePrediction->inner());

    m_predicting = true;
    while (!pl->m_playerDied) {
        using Mode = SLSettings::TrajectorySettings::Mode;

        int mode = pl->m_player1->m_jumpBuffered ? Mode::Release : Mode::Hold;

        auto pred = trajectory->simulate(pl, true, Mode::Player1 | mode, false,
                                         {
                                             .m_bypassConfig = true,
                                         });
        if (pred.score > bestPred) {
            bestPred = pred.score;
            bestFrame = this->getFrame();
        }

        if (bestPred >= lowestAcceptable) {
            break;
        }

        this->stepOnce();
        CCScheduler::get()->update(this->getPhysicsDt());
        iters++;
    }

    uint32_t steps = std::min(this->getFrame() - bestFrame, iters);

    this->backwardsStep(steps);

    while (this->getFrame() != bestFrame && steps != iters &&
           !Bot::get()->practiceFix().m_storedFrames.empty()) {
        this->backwardsStep();  // sometimes happens, oh well
    }

    m_predicting = false;

    // this->stepOnce();
    // CCScheduler::get()->update(this->getPhysicsDt());
}

// THE tps bypass

// DEAR GEODE REVIEWER or whoever is reading this
//
// this does NOT conflict with geode's hooking system / other mods
// these specific places in memory will NOT be hooked by other mods
// (at least i don't think so)
//
// everything DOES work fine with this bypass
// anyways yea yapping over heres the code:::::::::::::

constexpr int ACTIONMGR_UPDATE_OFFSET = 0x38B90;
static void* CCActionManager_update;

$execute {
    CCActionManager_update = reinterpret_cast<void*>(geode::base::getCocos() +
                                                     ACTIONMGR_UPDATE_OFFSET);
}

static void earlyUpdateMidhook(SafetyHookContext&) {
    Bot* bot = Bot::get();
    if (bot->updater().m_onlyRefresh) return;

    PlayLayer* pl = PlayLayer::get();
    if (!pl) return;

    if (!pl->m_playerDied && bot->updater().m_backwardsStepping->inner() && !Renderer::get()->isRecording()) {
        CheckpointObject* checkpoint = pl->createCheckpoint();
        checkpoint->retain();
        bot->practiceFix().saveState(
            checkpoint, Bot::get()->updater().m_frameOnLastAttempt);
    }
}

static void frameUpdateMidhook(SafetyHookContext& ctx) {
    SCOPED_TIMER("frameUpdate")

    auto bot = Bot::get();

    auto pl = GJBaseGameLayer::get();
    if (!pl) return;

    if (pl->m_resumeTimer > 0) {
        return;
    }

    // geode::log::info("le xmm13: {}", ctx.xmm13.f64[0]);

    if (!pl->m_playerDied) {
        if (PlayLayer::get()) {
            bot->updater().incrementFrame();
        }

        bot->hitboxes().saveToTrail(pl);

        auto trajectory = bot->trajectory().unsafeInner();
        if (trajectory && bot->updater().m_fullGamePrediction->inner() &&
            bot->updater().m_preventDeath->inner() && PlayLayer::get()) {
            using Mode = SLSettings::TrajectorySettings::Mode;

            {
                int mode = Mode::Player1;
                mode |=
                    pl->m_player1->m_jumpBuffered ? Mode::Hold : Mode::Release;
                auto pred = trajectory->simulate(pl, true, mode, false,
                                                 {
                                                     .m_bypassConfig = true,
                                                 });
                if (pred.score <= 2) {
                    bot->updater().setPaused(true);
                    // bot->updater().breakLoop();
                }

                if (bot->updater().m_autoFlipOnDeath->inner()) {
                    pl->queueButton(1, !pl->m_player1->m_jumpBuffered, false, 0.0);
                    bot->updater().setPaused(false);
                }
            }

            {
                int mode = Mode::Player2;
                mode |=
                    pl->m_player2->m_jumpBuffered ? Mode::Hold : Mode::Release;
                auto pred = trajectory->simulate(pl, false, mode, false,
                                                 {
                                                     .m_bypassConfig = true,
                                                 });
                if (pred.score <= 2) {
                    bot->updater().setPaused(true);
                    // bot->updater().breakLoop();
                }

                if (bot->updater().m_autoFlipOnDeath->inner()) {
                    pl->queueButton(1, !pl->m_player2->m_jumpBuffered, true, 0.0);
                    bot->updater().setPaused(false);
                }
            }
        }
    }

    if (auto pll = PlayLayer::get(); pl) {
        bot->autoclicker().update(pll);
    }
    // bot->updater().runFrozenTick();
}

static void physDtMidhook(SafetyHookContext& ctx) {
    auto pl = GJBaseGameLayer::get();
    if (!pl) return;

    auto bot = Bot::get();
    if (bot->updater().m_onlyRefresh) return;

    ctx.xmm1.f64[0] *= bot->updater().m_tps->inner() / 60.0;
    ctx.rip += 0x08;
}

// static void physStepCountMidhook(SafetyHookContext& ctx) {
//     auto pl = GJBaseGameLayer::get();
//     if (!pl) return;

//     auto bot = Bot::get();
//     bool fastBypass = bot->updater().useFastLockDelta() ||
//                       !bot->updater().m_lockDelta->inner();
//     if (!fastBypass && PlayLayer::get()) return;

//     // ctx.r11 = bot->updater().estimatedStepCount;
//     ctx.rdi = bot->updater().estimatedStepCount;
// }

// static void physStepCount2Midhook(SafetyHookContext& ctx) {
//     auto pl = GJBaseGameLayer::get();
//     if (!pl) return;

//     auto bot = Bot::get();
//     bool fastBypass = bot->updater().useFastLockDelta() ||
//                       !bot->updater().m_lockDelta->inner();
//     if (!fastBypass && PlayLayer::get()) return;

//     // ctx.r11 = std::min(bot->updater().estimatedStepCount, 1);
//     // ctx.rdx = std::min(bot->updater().estimatedStepCount, 1);
// }

static void physStepCountMidhook(SafetyHookContext& ctx) {
    auto bot = Bot::get();
    bool fastBypass = bot->updater().useFastLockDelta() ||
                      !bot->updater().m_lockDelta->inner();
    if (!fastBypass && PlayLayer::get()) return;

    // to explain what's going on here;
    // basically the game does an incrementing step count up to but not including 2
    //
    // the thing is, this can technically be negative
    // so for instance for a step count of 3
    // we wanna set it to -1
    // so the steps are gonna be: -1, 0, 1 respectively
    //
    // thank you robtop :mending_heart:
    ctx.rdx = 2 - bot->updater().estimatedStepCount; // lmao
}

static void restorePhysDtHook(SafetyHookContext& ctx) {
    auto pl = GJBaseGameLayer::get();
    if (!pl) return;

    auto bot = Bot::get();
    bool fastBypass = bot->updater().useFastLockDelta() ||
                      !bot->updater().m_lockDelta->inner();
    if (!fastBypass && PlayLayer::get()) return;

    double newDt = bot->updater().getPhysicsDt() * bot->updater().estimatedStepCount;

    ctx.xmm9.f64[0] = newDt;
}

$execute {
    util::midhook(geode::base::get() + 0x237A7C, "physDt", physDtMidhook);
    util::midhook(geode::base::get() + 0x237DCE, "physStepCount", physStepCountMidhook);
    util::midhook(geode::base::get() + 0x238F6E, "restorePhysDt", restorePhysDtHook);
    util::midhook(geode::base::get() + 0x237E42, "earlyUpdate", earlyUpdateMidhook);
    util::midhook(geode::base::get() + 0x238BAA, "frameUpdate", frameUpdateMidhook);

    Bot::get()->updater().m_speedhack->handle(
        [](double) { Bot::get()->updater().updateAudioSpeedhack(); });

    Bot::get()->updater().m_paused->handle([](bool& paused) {
        auto pl = PlayLayer::get();
        if (!pl || pl->m_isPaused || Renderer::get()->isRecording()) {
            paused = false;
        }
    });

    Bot::get()->updater().m_stepOnce->handle([](bool&) {
        auto& paused = Bot::get()->updater().m_paused;

        paused->inner() = true;
        paused->notifyChange();
    });

    Bot::get()->updater().m_stepBackwards->handle([](bool& paused) {
        auto pl = PlayLayer::get();
        if (pl && paused && Bot::get()->practiceFix().canRestoreState() && !Renderer::get()->isRecording()) {
            Bot::get()->updater().backwardsStep();
        }

        paused = false;
    });

    Bot::get()->updater().m_noMirror->handle([](bool& noMirror) {
        auto pl = PlayLayer::get();
        if (!pl) return;
        if (noMirror) {
            pl->m_gameState.m_levelFlipping = 0.0;
        } else {
            pl->m_gameState.m_levelFlipping =
                (float)pl->m_gameState.m_unkBool10;
        }
    });

    Bot::get()->updater().m_predictBestPath->handle(
        [](bool&) { Bot::get()->updater().findBestFrameCandidate(); });
}
