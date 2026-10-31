#include "GLES2Renderer.hpp"

#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;
using namespace bismuth::gles2;

// Android entry point. The desktop renderer in src/renderer/ is compiled out on
// Android (it needs desktop GL 4.3 compute shaders; GD creates an ES2 context).
class $modify(BismuthGLES2PlayLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        if (!Mod::get()->getSettingValue<bool>("mobile_batching"))
            return;

        if (!m_objectLayer || !m_objects) {
            log::warn("[gles2] no object layer, skipping batching");
            return;
        }

        GLES2Renderer::create(this);
    }

    void onQuit() {
        GLES2Renderer::destroyInstance();
        PlayLayer::onQuit();
    }
};
