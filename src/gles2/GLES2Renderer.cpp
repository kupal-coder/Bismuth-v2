#include "GLES2Renderer.hpp"

#include <Geode/binding/EffectGameObject.hpp>
#include <Geode/cocos/CCDirector.h>
#include <Geode/cocos/base_nodes/CCNode.h>
#include <Geode/cocos/sprite_nodes/CCSprite.h>
#include <Geode/cocos/textures/CCTexture2D.h>
#include <Geode/cocos/kazmath/include/kazmath/GL/matrix.h>
#include <Geode/cocos/kazmath/include/kazmath/mat4.h>

#include <algorithm>
#include <cstring>

using namespace geode::prelude;

namespace bismuth::gles2 {

// GLSL 1.00 (GLES2) / 1.10 (desktop) compatible: no #version, no layout qualifiers.
static const char* VERTEX_SRC =
    "attribute vec4 a_position;\n"
    "attribute vec4 a_color;\n"
    "attribute vec2 a_texCoord;\n"
    "uniform mat4 u_mvp;\n"
    "varying vec4 v_color;\n"
    "varying vec2 v_texCoord;\n"
    "void main() {\n"
    "    gl_Position = u_mvp * a_position;\n"
    "    v_color = a_color;\n"
    "    v_texCoord = a_texCoord;\n"
    "}\n";

static const char* FRAGMENT_SRC =
    "#ifdef GL_ES\n"
    "precision mediump float;\n"
    "#endif\n"
    "varying vec4 v_color;\n"
    "varying vec2 v_texCoord;\n"
    "uniform sampler2D u_texture;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(u_texture, v_texCoord) * v_color;\n"
    "}\n";

GLES2Renderer* GLES2Renderer::s_current = nullptr;

struct GLES2Renderer::BatchNode : cocos2d::CCNode {
    GLES2Renderer* owner = nullptr;
    SpriteBatch* batch = nullptr;

    void draw() override {
        if (!owner || !batch || batch->empty())
            return;

        float mvp[16];
        VisibleRect rect;
        owner->computeFrameState(mvp, rect);
        batch->draw(rect, owner->program(), mvp);
    }
};

GLES2Renderer* GLES2Renderer::get() {
    return s_current;
}

GLES2Renderer* GLES2Renderer::create(PlayLayer* layer) {
    if (s_current)
        destroyInstance();

    if (!layer || !layer->m_objectLayer)
        return nullptr;

    bool flipUv = Mod::get()->getSettingValue<bool>("mobile_uv_flip");
    auto* renderer = new GLES2Renderer(layer, flipUv);

    if (!renderer->m_program.initWithSource(VERTEX_SRC, FRAGMENT_SRC, "gles2_sprite")) {
        log::error("[gles2] failed to create sprite shader, batching disabled");
        delete renderer;
        return nullptr;
    }

    s_current = renderer;
    renderer->rebuild();
    return renderer;
}

void GLES2Renderer::destroyInstance() {
    if (!s_current)
        return;
    delete s_current;
    s_current = nullptr;
}

GLES2Renderer::~GLES2Renderer() {
    teardownBatches();

    // Objects are owned by the level; only restore what is still attached.
    for (auto* object : m_hidden) {
        if (object && object->getParent())
            object->setVisible(true);
    }
    m_hidden.clear();

    if (s_current == this)
        s_current = nullptr;
}

void GLES2Renderer::teardownBatches() {
    for (auto* node : m_nodes) {
        if (node && node->getParent())
            node->removeFromParentAndCleanup(true);
    }
    m_nodes.clear();

    for (auto* batch : m_batches)
        delete batch;
    m_batches.clear();

    m_built = false;
}

bool GLES2Renderer::canBatch(GameObject* object) {
    if (!object || object->m_objectID <= 0)
        return false;

    // Triggers, portals, orbs and anything else effect driven
    if (typeinfo_cast<EffectGameObject*>(object))
        return false;

    // A group means a trigger can move, rotate, scale, toggle or recolor it
    if (object->m_groupCount > 0)
        return false;

    if (object->m_usesAudioScale)
        return false;
    if (object->getHasRotateAction())
        return false;

    // Tinted-black sprites are produced by trigger driven object modes
    if (object->m_isObjectBlack || object->m_isColorSpriteBlack)
        return false;

    if (!object->isVisible())
        return false;

    if (object->numberOfRunningActions() > 0)
        return false;

    // Detached colour sprites live outside the object's transform chain
    if (object->m_colorSprite && object->m_colorSprite->getParent() != object)
        return false;

    return true;
}

static inline u32 textureName(cocos2d::CCTexture2D* texture) {
    return texture ? (u32)texture->getName() : 0;
}

void GLES2Renderer::addSprite(
    GameObject* object,
    cocos2d::CCSprite* sprite,
    const cocos2d::CCAffineTransform& accumulated,
    int zOrder,
    bool isGlow,
    std::vector<SpriteQuad>& out
) {
    if (!sprite)
        return;

    // Same accumulation order as ObjectUtils::unpackSpriteRecursively
    cocos2d::CCAffineTransform transform =
        CCAffineTransformConcat(sprite->nodeToParentTransform(), accumulated);

    auto* children = sprite->getChildren();

    if (children) {
        for (auto* child : CCArrayExt<cocos2d::CCSprite*>(children)) {
            if (child && child->getZOrder() < 0)
                addSprite(object, child, transform, zOrder, child == object->m_glowSprite, out);
        }
    }

    auto* texture = sprite->getTexture();
    if (!sprite->getDontDraw() && sprite->isVisible() && texture) {
        cocos2d::CCRect crop = sprite->getTextureRect();

        if (crop.size.width > 0.f && crop.size.height > 0.f) {
            cocos2d::CCPoint origin = CCPointApplyAffineTransform(sprite->getOffsetPosition(), transform);
            cocos2d::CCPoint right = { transform.a * crop.size.width, transform.b * crop.size.width };
            cocos2d::CCPoint up = { transform.c * crop.size.height, transform.d * crop.size.height };

            if (sprite->isFlipX()) {
                origin.x += right.x;
                origin.y += right.y;
                right.x = -right.x;
                right.y = -right.y;
            }
            if (sprite->isFlipY()) {
                origin.x += up.x;
                origin.y += up.y;
                up.x = -up.x;
                up.y = -up.y;
            }

            float csf = cocos2d::CCDirector::get()->getContentScaleFactor();
            float tw = (float)texture->getPixelsWide();
            float th = (float)texture->getPixelsHigh();

            cocos2d::CCPoint uvBottomLeft;
            cocos2d::CCPoint uvRight;
            cocos2d::CCPoint uvUp;

            if (!sprite->isTextureRectRotated()) {
                uvBottomLeft = { crop.origin.x, crop.origin.y + crop.size.height };
                uvRight = { crop.size.width, 0.f };
                uvUp = { 0.f, -crop.size.height };
            }
            else {
                uvBottomLeft = { crop.origin.x, crop.origin.y };
                uvRight = { 0.f, crop.size.width };
                uvUp = { crop.size.height, 0.f };
            }

            cocos2d::CCPoint uvFactor = { csf / tw, csf / th };
            uvBottomLeft.x *= uvFactor.x;
            uvBottomLeft.y *= uvFactor.y;
            uvRight.x *= uvFactor.x;
            uvRight.y *= uvFactor.y;
            uvUp.x *= uvFactor.x;
            uvUp.y *= uvFactor.y;

            if (m_flipUv) {
                uvBottomLeft.y = 1.f - uvBottomLeft.y;
                uvUp.y = -uvUp.y;
            }

            ccColor3B color = sprite->getColor();
            GLubyte opacity = sprite->getDisplayedOpacity();

            SpriteQuad quad;
            quad.texture = textureName(texture);
            quad.zOrder = zOrder;
            quad.blend = isGlow ? 1 : 0;
            quad.r = color.r;
            quad.g = color.g;
            quad.b = color.b;
            quad.a = opacity;

            // BL, BR, TL, TR
            cocos2d::CCPoint br = { origin.x + right.x, origin.y + right.y };
            cocos2d::CCPoint tl = { origin.x + up.x, origin.y + up.y };
            cocos2d::CCPoint tr = { br.x + up.x, br.y + up.y };

            quad.x[0] = origin.x; quad.y[0] = origin.y;
            quad.x[1] = br.x;     quad.y[1] = br.y;
            quad.x[2] = tl.x;     quad.y[2] = tl.y;
            quad.x[3] = tr.x;     quad.y[3] = tr.y;

            quad.u[0] = uvBottomLeft.x;                 quad.v[0] = uvBottomLeft.y;
            quad.u[1] = uvBottomLeft.x + uvRight.x;     quad.v[1] = uvBottomLeft.y + uvRight.y;
            quad.u[2] = uvBottomLeft.x + uvUp.x;        quad.v[2] = uvBottomLeft.y + uvUp.y;
            quad.u[3] = uvBottomLeft.x + uvRight.x + uvUp.x;
            quad.v[3] = uvBottomLeft.y + uvRight.y + uvUp.y;

            out.push_back(quad);
        }
    }

    if (children) {
        for (auto* child : CCArrayExt<cocos2d::CCSprite*>(children)) {
            if (child && child->getZOrder() >= 0)
                addSprite(object, child, transform, zOrder, child == object->m_glowSprite, out);
        }
    }
}

void GLES2Renderer::collectQuads(std::vector<SpriteQuad>& out) {
    auto* objectLayer = m_layer->m_objectLayer;
    if (!objectLayer || !m_layer->m_objects)
        return;

    for (auto* object : CCArrayExt<GameObject*>(m_layer->m_objects)) {
        if (!canBatch(object))
            continue;

        bool animatedChild = false;
        if (auto* children = object->getChildren()) {
            for (auto* child : CCArrayExt<cocos2d::CCNode*>(children)) {
                if (child && child->numberOfRunningActions() > 0) {
                    animatedChild = true;
                    break;
                }
            }
        }
        if (animatedChild)
            continue;

        cocos2d::CCAffineTransform objectToLayer = CCAffineTransformMakeIdentity();
        auto* parent = object->getParent();
        if (parent && parent != objectLayer) {
            objectToLayer = CCAffineTransformConcat(
                CCAffineTransformInvert(objectLayer->nodeToWorldTransform()),
                parent->nodeToWorldTransform()
            );
        }

        usize before = out.size();
        int zOrder = object->getZOrder();

        // Root call: accumulate from the object's own transform downwards
        auto emit = [&](cocos2d::CCSprite* sprite, const cocos2d::CCAffineTransform& accum, bool glow) {
            addSprite(object, sprite, accum, zOrder, glow, out);
        };
        emit(object, CCAffineTransformMakeIdentity(), false);

        // Re-map the quads we just produced into objectLayer space
        for (usize i = before; i < out.size(); i++) {
            auto& quad = out[i];
            for (int c = 0; c < 4; c++) {
                cocos2d::CCPoint p = CCPointApplyAffineTransform({ quad.x[c], quad.y[c] }, objectToLayer);
                quad.x[c] = p.x;
                quad.y[c] = p.y;
            }
        }

        if (out.size() > before) {
            m_hidden.push_back(object);
            object->setVisible(false);
        }
    }
}

bool GLES2Renderer::buildBatches(std::vector<SpriteQuad>& quads) {
    if (quads.empty())
        return false;

    std::sort(quads.begin(), quads.end(), [](const SpriteQuad& a, const SpriteQuad& b) {
        if (a.zOrder != b.zOrder) return a.zOrder < b.zOrder;
        if (a.texture != b.texture) return a.texture < b.texture;
        return a.blend < b.blend;
    });

    auto* objectLayer = m_layer->m_objectLayer;

    usize i = 0;
    while (i < quads.size()) {
        usize end = i;
        while (end < quads.size() && quads[end].zOrder == quads[i].zOrder
            && quads[end].texture == quads[i].texture && quads[end].blend == quads[i].blend
            && (end - i) < MAX_QUADS_PER_BATCH) {
            end++;
        }

        std::vector<const SpriteQuad*> ptrs;
        ptrs.reserve(end - i);
        for (usize k = i; k < end; k++)
            ptrs.push_back(&quads[k]);

        auto* batch = new SpriteBatch();
        if (!batch->build(ptrs.data(), ptrs.size(), quads[i].texture, quads[i].zOrder, quads[i].blend)) {
            delete batch;
            i = end;
            continue;
        }

        auto* node = new BatchNode();
        node->owner = this;
        node->batch = batch;

        objectLayer->addChild(node, quads[i].zOrder);

        m_batches.push_back(batch);
        m_nodes.push_back(node);

        i = end;
    }

    return !m_batches.empty();
}

void GLES2Renderer::rebuild() {
    teardownBatches();

    std::vector<SpriteQuad> quads;
    collectQuads(quads);

    m_built = buildBatches(quads);

    log::info("[gles2] batched {} objects into {} quads / {} draw calls",
        m_hidden.size(), quads.size(), m_batches.size());

    if (!m_built)
        log::warn("[gles2] nothing was batchable in this level");
}

void GLES2Renderer::computeFrameState(float* mvp16, VisibleRect& rect) {
    kmMat4 projection, modelview, mvp;
    kmGLGetMatrix(KM_GL_PROJECTION, &projection);
    kmGLGetMatrix(KM_GL_MODELVIEW, &modelview);
    kmMat4Multiply(&mvp, &projection, &modelview);
    std::memcpy(mvp16, mvp.mat, sizeof(float) * 16);

    auto* objectLayer = m_layer->m_objectLayer;
    if (!objectLayer) {
        rect = {};
        return;
    }

    auto winSize = cocos2d::CCDirector::get()->getWinSize();
    const float margin = 128.f;

    cocos2d::CCPoint a = objectLayer->convertToNodeSpace({ -margin, -margin });
    cocos2d::CCPoint b = objectLayer->convertToNodeSpace({ winSize.width + margin, winSize.height + margin });

    rect.x0 = std::min(a.x, b.x);
    rect.y0 = std::min(a.y, b.y);
    rect.x1 = std::max(a.x, b.x);
    rect.y1 = std::max(a.y, b.y);
}

} // namespace bismuth::gles2
