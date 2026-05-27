#include "hitboxes.hpp"
#include <Geode/Geode.hpp>
#include "bot/bot.hpp"
#include "bot/updater.hpp"
#include "ccTypes.h"
#include "settings/settings.hpp"
#include "trajectory/trajectory.hpp"

#define CC_COLOR(color_type) *reinterpret_cast<cocos2d::ccColor4F*>(settings.categories[color_type].colors.data())
#define CC_FILL_COLOR(color_type) { \
    settings.categories[color_type].colors.data()[0], \
    settings.categories[color_type].colors.data()[1], \
    settings.categories[color_type].colors.data()[2], \
    settings.categories[color_type].colors.data()[3] * (float)settings.categories[color_type].fillOpacity, \
}
#define HB_ENABLED(_t) settings.categories[_t].enabled

static cocos2d::CCRect usingWidth(const cocos2d::CCRect& old, const float width) {
    if (width == 0.0) return old;

    cocos2d::CCRect rect = old;
    rect.origin.x += width;
    rect.origin.y += width;
    rect.size.width -= width * 2.0;
    rect.size.height -= width * 2.0;
    return rect;
}

static void drawPlayerHitbox(cocos2d::CCDrawNode* node, GJBaseGameLayer* pl,
                             PlayerObject* player,
                             SLSettings::HitboxSettings& settings) {
    using Type = SLSettings::HitboxSettings::Type;
    if (!player) return;

    const float width = settings.width / pl->m_gameState.m_cameraZoom;

    auto rect = usingWidth(player->getObjectRect(), width);
    auto scaled = usingWidth(player->getObjectRect(0.3, 0.3), width);

    if (auto ob = player->getOrientedBox(); ob && HB_ENABLED(Type::PlayerRotated)) {
        node->drawPolygon(ob->m_corners.data(), 4, CC_FILL_COLOR(Type::PlayerRotated), width,
            CC_COLOR(Type::PlayerRotated));
    }

    float radius = player->getObjectRect().size.width / 2.0;
    if (radius > 0 && HB_ENABLED(Type::PlayerCircle)) {
        int segments = (int)std::max(radius, 8.0f) * 3.0;

        node->drawCircle(player->getPosition(), radius,
                         {0.0, 0.0, 0.0, 0.0}, width,
                         CC_COLOR(Type::PlayerCircle), segments);
    }

    if (HB_ENABLED(Type::Player)) {
        node->drawRect(rect, CC_FILL_COLOR(Type::Player), width, CC_COLOR(Type::Player));
    }
    if (HB_ENABLED(Type::PlayerInner)) {
        node->drawRect(scaled, CC_FILL_COLOR(Type::PlayerInner), width, CC_COLOR(Type::PlayerInner));
    }
}

inline bool HitboxTrailUnit::shouldDraw(float minX, float maxX, float minY, float maxY) {
    if (m_rect.getMaxX() < minX || m_rect.getMinX() > maxX || m_rect.getMinY() > maxY || m_rect.getMaxY() < minY) return false;

    return true;
}

void HitboxTrailUnit::draw(cocos2d::CCDrawNode* node, float width, float* colors, float fillOpacity) {
    cocos2d::CCRect rect = usingWidth(m_rect, width);

    node->drawRect(rect, {colors[0], colors[1], colors[2], colors[3] * fillOpacity}, width, *reinterpret_cast<cocos2d::ccColor4F*>(colors));
}

void HitboxTrailUnit::drawRotated(cocos2d::CCDrawNode* node, float width, float* colors, float fillOpacity) {
    node->drawPolygon(m_rotated.data(), 4, {colors[0], colors[1], colors[2], colors[3] * fillOpacity}, width,
        *reinterpret_cast<cocos2d::ccColor4F*>(colors));
}

void HitboxTrailUnit::drawInner(cocos2d::CCDrawNode* node, float width, float* colors, float fillOpacity) {
    cocos2d::CCRect scaled = usingWidth(m_scaled, width);
    node->drawRect(scaled, {colors[0], colors[1], colors[2], colors[3] * fillOpacity}, width, *reinterpret_cast<cocos2d::ccColor4F*>(colors));
}

void HitboxTrailUnit::drawCircle(cocos2d::CCDrawNode* node, float width,
                                 float* colors, float fillOpacity) {
    cocos2d::CCRect rect = usingWidth(m_rect, width);
    float radius = rect.size.width / 2.0;
    if (radius > 0) {
        int segments = (int)std::max(radius, 8.0f) * 3.0;

        node->drawCircle(cocos2d::CCPoint{rect.getMidX(), rect.getMidY()}, radius,
                         {colors[0], colors[1], colors[2], colors[3] * fillOpacity}, width,
                         *reinterpret_cast<cocos2d::ccColor4F*>(colors), segments);
    }
}

void Hitboxes::init(GJBaseGameLayer* pl) {
    if (m_initialized) {
        this->destroy(); // probably worth it to just recreate
    }

    m_drawNode = HitboxesDrawNode::create();
    m_drawNode->retain();
    m_drawNode->setID("hitbox-node"_spr);
    m_drawNode->setBlendFunc({GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA});

    m_solidTrailDrawNode = HitboxesDrawNode::create();
    m_solidTrailDrawNode->retain();
    m_solidTrailDrawNode->setID("hitbox-trail-node-solid"_spr);
    m_solidTrailDrawNode->setBlendFunc({GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA});

    m_circleTrailDrawNode = HitboxesDrawNode::create();
    m_circleTrailDrawNode->retain();
    m_circleTrailDrawNode->setID("hitbox-trail-node-circle"_spr);
    m_circleTrailDrawNode->setBlendFunc({GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA});

    m_innerTrailDrawNode = HitboxesDrawNode::create();
    m_innerTrailDrawNode->retain();
    m_innerTrailDrawNode->setID("hitbox-trail-node-inner"_spr);
    m_innerTrailDrawNode->setBlendFunc({GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA});

    m_rotatedTrailDrawNode = HitboxesDrawNode::create();
    m_rotatedTrailDrawNode->retain();
    m_rotatedTrailDrawNode->setID("hitbox-trail-node-rotated"_spr);
    m_rotatedTrailDrawNode->setBlendFunc({GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA});

    pl->m_debugDrawNode->getParent()->addChild(
        m_drawNode, pl->m_uiLayer->getZOrder() + 10000);
    pl->m_debugDrawNode->getParent()->addChild(
        m_solidTrailDrawNode, pl->m_uiLayer->getZOrder() + 9998);
    pl->m_debugDrawNode->getParent()->addChild(
        m_rotatedTrailDrawNode, pl->m_uiLayer->getZOrder() + 9996);
    pl->m_debugDrawNode->getParent()->addChild(
        m_innerTrailDrawNode, pl->m_uiLayer->getZOrder() + 9999);
    pl->m_debugDrawNode->getParent()->addChild(
        m_circleTrailDrawNode, pl->m_uiLayer->getZOrder() + 9997);

    m_initialized = true;
}

static void iterateObjects(GJBaseGameLayer* pl,
                           std::function<void(GameObject*)> callback) {
    auto& sections = pl->m_sections;

    for (int i = pl->m_leftSectionIndex;
         i <= pl->m_rightSectionIndex && static_cast<size_t>(i) < sections.size(); i++) {
        auto horizontalSection = sections[i];
        if (!horizontalSection) continue;

        for (int j = pl->m_bottomSectionIndex;
             j <= pl->m_topSectionIndex && static_cast<size_t>(j) < horizontalSection->size(); j++) {
            auto section = horizontalSection->at(j);
            if (!section) continue;

            for (int k = 0; k < pl->m_sectionSizes[i]->at(j); k++) {
                auto object = section->at(k);
                if (!object) continue;

                callback(object);
            }
        }
    }
}

const std::array ALWAYS_ALLOWED_MODIFIERS = {200,  201,  202, 203,
                                             1334, 1816, 3643};

static void drawObjectHitbox(cocos2d::CCDrawNode* node, GJBaseGameLayer* pl,
                             GameObject* object,
                             SLSettings::HitboxSettings& settings) {
    using Type = SLSettings::HitboxSettings::Type;
    const float width = settings.width / pl->m_gameState.m_cameraZoom;

    if (object->m_isGroupDisabled || !object->m_isActivated ||
        object->m_objectType == GameObjectType::Decoration)
        return;

    if ((object == pl->m_player1 || object == pl->m_player2) ||
        Bot::get()->trajectory().isFakePlayer((PlayerObject*)object)) {
        return;
    }

    switch (object->m_objectType) {
        default: {
            if (object->hasBeenActivatedByPlayer(pl->m_player1) &&
                (!pl->m_gameState.m_isDualMode ||
                 object->hasBeenActivatedByPlayer(pl->m_player2))) {
                return;
            }

            if (object->m_objectType == GameObjectType::Modifier &&
                std::ranges::find(ALWAYS_ALLOWED_MODIFIERS, object->m_objectID) ==
                    ALWAYS_ALLOWED_MODIFIERS.end()) {
                if (auto obj =
                        geode::cast::typeinfo_cast<EffectGameObject*>(object);
                        !obj->m_isTouchTriggered) {
                    return;
                }
            }

            bool isObjectRectDirty = object->m_isObjectRectDirty;
            bool boxOffsetCalculated = object->m_boxOffsetCalculated;

            if (auto ob = object->m_orientedBox; ob ) {
                if (HB_ENABLED(Type::Interactable)) {
                    node->drawPolygon(ob->m_corners.data(), 4, CC_FILL_COLOR(Type::Interactable),
                                      width, CC_COLOR(Type::Interactable));
                }
            } else {
                if (HB_ENABLED(Type::Interactable)) {
                    node->drawRect(usingWidth(object->getObjectRect(), width), CC_FILL_COLOR(Type::Interactable),
                                width, CC_COLOR(Type::Interactable));
                }
            }

            object->m_isObjectRectDirty = isObjectRectDirty;
            object->m_boxOffsetCalculated = boxOffsetCalculated;
            break;
        }

        case GameObjectType::CollisionObject: {
            return;  // this took me too long to figure out tbh
        }

        case GameObjectType::Solid: {
            if (object->m_isPassable) {
                if (HB_ENABLED(Type::Passable)) {
                    node->drawRect(usingWidth(object->getObjectRect(), width), CC_FILL_COLOR(Type::Passable),
                                width, CC_COLOR(Type::Passable));
                }
            } else {
                if (HB_ENABLED(Type::Solid)) {
                    node->drawRect(usingWidth(object->getObjectRect(), width), CC_FILL_COLOR(Type::Solid),
                                width, CC_COLOR(Type::Solid));
                }
            }

            break;
        }

        case GameObjectType::Slope: {
            auto rect = object->getObjectRect();
            std::array verts = {
                rect.origin,
                cocos2d::CCPoint{rect.getMinX(), rect.getMaxY()},
                cocos2d::CCPoint{rect.getMaxX(), rect.getMinY()},
            };

            auto max = cocos2d::CCPoint(rect.getMaxX(), rect.getMaxY());
            switch (object->m_slopeDirection) {
                case 0:
                case 7:
                    verts[1] = max;
                    break;
                case 1:
                case 5:
                    verts[0] = max;
                    break;
                case 3:
                case 6:
                    verts[2] = max;
                    break;
            };

            if (object->m_isPassable) {
                if (HB_ENABLED(Type::Passable)) {
                    node->drawPolygon(verts.data(), 3, CC_FILL_COLOR(Type::Passable), width,
                        CC_COLOR(Type::Passable));
                }
            } else {
                if (HB_ENABLED(Type::Solid)) {
                    node->drawPolygon(verts.data(), 3, CC_FILL_COLOR(Type::Solid), width,
                        CC_COLOR(Type::Solid));
                }
            }

            break;
        }

        case GameObjectType::AnimatedHazard:
        case GameObjectType::Hazard: {
            if (!object->m_isActivated) return;
            if (object == pl->m_anticheatSpike) return;
            if (!HB_ENABLED(Type::Hazard)) return;

            // ripped from updateDebugDraw
            bool isObjectRectDirty = object->m_isObjectRectDirty;
            bool boxOffsetCalculated = object->m_boxOffsetCalculated;

            float radius = object->m_objectRadius *
                           std::max(object->m_scaleX, object->m_scaleY);
            if (radius > 0) {
                int segments = (int)std::max(radius, 8.0f);

                node->drawCircle(object->getPosition(), radius,
                                 CC_FILL_COLOR(Type::Hazard), width,
                                 CC_COLOR(Type::Hazard), segments);
            } else if (auto ob = object->m_orientedBox; ob) {
                node->drawPolygon(ob->m_corners.data(), 4, CC_FILL_COLOR(Type::Hazard),
                                  width, CC_COLOR(Type::Hazard));
            } else {
                node->drawRect(usingWidth(object->getObjectRect(), width), CC_FILL_COLOR(Type::Hazard),
                               width, CC_COLOR(Type::Hazard));
            }

            object->m_isObjectRectDirty = isObjectRectDirty;
            object->m_boxOffsetCalculated = boxOffsetCalculated;

            break;
        }
    }
}

void Hitboxes::draw(GJBaseGameLayer* pl) {
    if (!m_initialized) return;


    m_drawNode->clear();
    m_solidTrailDrawNode->clear();
    m_rotatedTrailDrawNode->clear();
    m_innerTrailDrawNode->clear();
    m_circleTrailDrawNode->clear();

    if (!m_enabled->inner()) return;

    if (m_trailEnabled->inner()) {
        float scale = pl->m_objectLayer->getScale();
        float minX = -pl->m_objectLayer->getPositionX() / scale - pl->m_cameraWidth;
        float maxX = -pl->m_objectLayer->getPositionX() / scale + pl->m_cameraWidth;
        float minY = -pl->m_objectLayer->getPositionY() / scale - pl->m_cameraHeight;
        float maxY = -pl->m_objectLayer->getPositionY() / scale + pl->m_cameraHeight;
        // auto rect = cocos2d::CCRect(
        //     minX, minY,
        //     maxX - minX, maxY - minY
        // );

        // m_innerTrailDrawNode->enableDrawArea(rect);
        // m_rotatedTrailDrawNode->enableDrawArea(rect);
        // m_solidTrailDrawNode->enableDrawArea(rect);

        auto& settings = SLSettings::get()->hitboxes;
        using Type = SLSettings::HitboxSettings::Type;

        const float width = settings.width / pl->m_gameState.m_cameraZoom;
        float frequencyLevel = (Bot::get()->updater().m_tps->inner() + 500.0) / 1000.0;
        frequencyLevel /= scale;
        if (scale >= 4) {
            frequencyLevel = 1;
        }

        int frequency = std::max((int)frequencyLevel, 1);

        // geode::log::info("drawing rotated trail...");
        if (HB_ENABLED(Type::PlayerRotated)) {
            float* colors = settings.categories[Type::PlayerRotated].colors.data();
            float fillOpacity = settings.categories[Type::PlayerRotated].fillOpacity;
            for (size_t i = 0; i < m_trailP1.size(); i += frequency) {
                auto& unit = m_trailP1[i];
                if (unit.shouldDraw(minX, maxX, minY, maxY)) {
                    unit.drawRotated(this->m_rotatedTrailDrawNode, width, colors, fillOpacity);
                }
            }

            for (size_t i = 0; i < m_trailP2.size(); i += frequency) {
                auto& unit = m_trailP2[i];
                if (unit.shouldDraw(minX, maxX, minY, maxY)) {
                    unit.drawRotated(this->m_rotatedTrailDrawNode, width, colors, fillOpacity);
                }
            }
        }

        // geode::log::info("drawing normal trail...");
        if (HB_ENABLED(Type::Player)) {
            float* colors = settings.categories[Type::Player].colors.data();
            float fillOpacity = settings.categories[Type::Player].fillOpacity;
            for (size_t i = 0; i < m_trailP1.size(); i += frequency) {
                auto& unit = m_trailP1[i];
                if (unit.shouldDraw(minX, maxX, minY, maxY)) {
                    unit.draw(this->m_solidTrailDrawNode, width, colors, fillOpacity);
                }
            }

            for (size_t i = 0; i < m_trailP2.size(); i += frequency) {
                auto& unit = m_trailP2[i];
                if (unit.shouldDraw(minX, maxX, minY, maxY)) {
                    unit.draw(this->m_solidTrailDrawNode, width, colors, fillOpacity);
                }
            }
        }

        if (HB_ENABLED(Type::PlayerCircle)) {
            float* colors = settings.categories[Type::PlayerCircle].colors.data();
            float fillOpacity = settings.categories[Type::PlayerCircle].fillOpacity;
            for (size_t i = 0; i < m_trailP1.size(); i += frequency) {
                auto& unit = m_trailP1[i];
                if (unit.shouldDraw(minX, maxX, minY, maxY)) {
                    unit.drawCircle(this->m_circleTrailDrawNode, width, colors, fillOpacity);
                }
            }

            for (size_t i = 0; i < m_trailP2.size(); i += frequency) {
                auto& unit = m_trailP2[i];
                if (unit.shouldDraw(minX, maxX, minY, maxY)) {
                    unit.drawCircle(this->m_circleTrailDrawNode, width, colors, fillOpacity);
                }
            }
        }

        // geode::log::info("drawing inner trail...");
        if (HB_ENABLED(Type::PlayerInner)) {
            float* colors = settings.categories[Type::PlayerInner].colors.data();
            float fillOpacity = settings.categories[Type::PlayerInner].fillOpacity;
            for (size_t i = 0; i < m_trailP1.size(); i += frequency) {
                auto& unit = m_trailP1[i];
                if (unit.shouldDraw(minX, maxX, minY, maxY)) {
                    unit.drawInner(this->m_innerTrailDrawNode, width, colors, fillOpacity);
                }
            }

            for (size_t i = 0; i < m_trailP2.size(); i += frequency) {
                auto& unit = m_trailP2[i];
                if (unit.shouldDraw(minX, maxX, minY, maxY)) {
                    unit.drawInner(this->m_innerTrailDrawNode, width, colors, fillOpacity);
                }
            }
        }
    }

    iterateObjects(pl, [&](GameObject* object) {
        drawObjectHitbox(this->m_drawNode, pl, object,
                         SLSettings::get()->hitboxes);
    });

    drawPlayerHitbox(this->m_drawNode, pl, pl->m_player1,
                     SLSettings::get()->hitboxes);
    if (pl->m_gameState.m_isDualMode) {
        drawPlayerHitbox(this->m_drawNode, pl, pl->m_player2,
                         SLSettings::get()->hitboxes);
    }
}

void Hitboxes::saveToTrail(GJBaseGameLayer* pl) {
    if (pl->m_levelEndAnimationStarted) {
        return; // don't
    }

    m_trailP1.push_back({
        .m_rect = pl->m_player1->getObjectRect(),
        .m_scaled = pl->m_player1->getObjectRect(0.3, 0.3),
        .m_rotated = pl->m_player1->getOrientedBox()->m_corners,
    });
    m_trailP2.push_back({
        .m_rect = pl->m_player2->getObjectRect(),
        .m_scaled = pl->m_player2->getObjectRect(0.3, 0.3),
        .m_rotated = pl->m_player2->getOrientedBox()->m_corners,
    });

    if (m_trailP1.size() > 69420) {
        m_trailP1.pop_front();
    }

    if (m_trailP2.size() > 69420) {
        m_trailP2.pop_front();
    }
}

void Hitboxes::clearTrail() {
    m_trailP1.clear();
    m_trailP2.clear();
}

void Hitboxes::destroy() {
    if (!m_initialized) return;

    CC_SAFE_RELEASE(m_drawNode);
    m_initialized = false;
}
