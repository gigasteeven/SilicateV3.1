#include <Geode/Geode.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>
#include <deque>

#include "module/module.hpp"
#include "settings/settings.hpp"
#include "shared/value/value.hpp"

struct HitboxTrailUnit {
    cocos2d::CCRect m_rect;
    cocos2d::CCRect m_scaled;
    std::array<cocos2d::CCPoint, 4> m_rotated;

    // inline this shit
    inline bool shouldDraw(float minX, float maxX, float minY, float maxY);
    void draw(cocos2d::CCDrawNode* node, float width, float* colors, float fillOpacity);
    void drawRotated(cocos2d::CCDrawNode* node, float width, float* colors, float fillOpacity);
    void drawInner(cocos2d::CCDrawNode* node, float width, float* colors, float fillOpacity);
    void drawCircle(cocos2d::CCDrawNode* node, float width, float* colors, float fillOpacity);
};

class Hitboxes : mod::Module {
    INIT_MODULE(Hitboxes)

    class HitboxesDrawNode : public cocos2d::CCDrawNode {
       public:
        static HitboxesDrawNode* create() {
            HitboxesDrawNode* node = new HitboxesDrawNode();

            if (node && node->init()) {
                node->autorelease();
                node->m_bUseArea = false;
            } else {
                CC_SAFE_DELETE(node);
            }

            return node;
        }
    };

    HitboxesDrawNode* m_drawNode;

    HitboxesDrawNode* m_solidTrailDrawNode;
    HitboxesDrawNode* m_rotatedTrailDrawNode;
    HitboxesDrawNode* m_innerTrailDrawNode;
    HitboxesDrawNode* m_circleTrailDrawNode;

    bool _enabled = false;
    std::deque<HitboxTrailUnit> m_trailP1;
    std::deque<HitboxTrailUnit> m_trailP2;

   public:
    bool m_initialized = false;

    SLValuePtr<bool> m_enabled =
        SLValue<bool>::create("hitboxes.enabled", &_enabled);
    SLValuePtr<bool> m_trailEnabled = SLValue<bool>::create(
        "hitboxes.trail_enabled", &SLSettings::get()->hitboxes.trailEnabled);
    SLValuePtr<double> m_width = SLValue<double>::create(
        "hitboxes.width", &SLSettings::get()->hitboxes.width);

    void init(GJBaseGameLayer* pl);

    void clearTrail();
    void saveToTrail(GJBaseGameLayer* pl);

    void draw(GJBaseGameLayer* pl);
    void destroy();
};
