#pragma once

#include <Geode/Geode.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/cocos/platform/CCGL.h>
#include "GLES2Shader.hpp"
#include "SpriteBatch.hpp"
#include <vector>

namespace bismuth::gles2 {

// GLES2-legal replacement for the desktop compute renderer.
// Bakes every "safe static" object into texture/z sorted triangle batches and
// culls them per frame on the CPU. Requires no compute shaders, SSBOs or GL 4.3.
class GLES2Renderer {
public:
    static GLES2Renderer* get();
    static GLES2Renderer* create(PlayLayer* layer);
    static void destroyInstance();

    ~GLES2Renderer();

    // Rebuilds geometry from the current level objects. Safe to call repeatedly.
    void rebuild();

    inline PlayLayer* getPlayLayer() const { return m_layer; }
    inline ShaderProgram& program() { return m_program; }

    void computeFrameState(float* mvp16, VisibleRect& rect);

private:
    GLES2Renderer(PlayLayer* layer, bool flipUv) : m_layer(layer), m_flipUv(flipUv) {}

    static bool canBatch(GameObject* object);
    void collectQuads(std::vector<SpriteQuad>& out);
    void addSprite(
        GameObject* object, cocos2d::CCSprite* sprite, const cocos2d::CCAffineTransform& accumulated,
        int zOrder, bool isGlow, std::vector<SpriteQuad>& out
    );
    bool buildBatches(std::vector<SpriteQuad>& quads);
    void teardownBatches();

    struct BatchNode;

    static GLES2Renderer* s_current;

    PlayLayer* m_layer = nullptr;
    ShaderProgram m_program;
    bool m_flipUv = false;
    std::vector<SpriteBatch*> m_batches;
    std::vector<cocos2d::CCNode*> m_nodes;
    std::vector<GameObject*> m_hidden;
    bool m_built = false;
};

} // namespace bismuth::gles2
