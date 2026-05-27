#include <Geode/Geode.hpp>
#include <Geode/modify/CCScheduler.hpp>

using namespace geode::prelude;

#include "bot/bot.hpp"
// #include "recorder/recorder.hpp"

$on_mod(Loaded) { Bot::get()->initialize(); }
