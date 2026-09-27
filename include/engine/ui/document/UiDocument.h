// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_UIDOCUMENT_H
#define CONCORD_UIDOCUMENT_H

#include "Concord/CExport.h"
#include "engine/core/Color.h"
#include "engine/core/Vec2.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Concord {
class Window;

/** Controls supported by the portable .yu document format. */
enum class UiElementKind { Panel, Label, Button, Checkbox, Slider, TextInput, Progress };
/** Optional engine window command performed by an activated button. */
enum class UiAction { None, MinimizeWindow, ToggleMaximizeWindow, ToggleFullscreenWindow, CloseWindow };

/**
 * Where a document or control sits.
 * On a document, Free is a rectangle and Fill covers the host. On a control,
 * Free keeps anchors and the other values dock inside the parent.
 */
enum class UiPlace { Free, Fill, Top, Bottom, Left, Right, Center };

/** Authored control. Positions and dimensions use document reference pixels. */
struct UiElement {
    std::string id = "element";
    std::string parent;
    std::string text = "Button";
    std::string input;
    UiElementKind kind = UiElementKind::Button;
    Vec2 position{0, 0};
    Vec2 size{160, 48};
    Vec2 anchor{0, 0};
    ColorRGBA color = 0xffe4e6ea;
    ColorRGBA background = 0xff2a2e34;
    float rounding = 2;
    UiPlace place = UiPlace::Free;
    float fontScale = 1;
    float value = 0;
    bool visible = true;
    bool enabled = true;
    UiAction action = UiAction::None;
};

/** Draws inside the caller's active ImGui frame/window; owns no platform context. */
struct UiDrawDesc {
    Vec2 position{0, 0};
    Vec2 size{1280, 720};
    bool interactive = true;
    Window* window = nullptr;
};

/** Resolved bounds in screen pixels, aligned by index with UiDocument::elements. */
struct UiElementLayout {
    usize index = 0;
    Vec2 position{};
    Vec2 size{};
    Vec2 clipPosition{};
    Vec2 clipSize{};
    bool visible = true;
    bool enabled = true;
};

/** A click or changed persistent control value reported during Draw. */
enum class UiEventKind { Clicked, Changed };
/** Event payload; text input uses input, checkbox and slider use value. */
struct UiEvent {
    std::string id;
    UiEventKind kind = UiEventKind::Clicked;
    float value = 0;
    std::string input;
    UiAction action = UiAction::None;
};

/** Copyable editable hierarchy, serialized as a bounded versioned .yu text document. */
struct CENGINE_API UiDocument {
    Vec2 referenceSize{1280, 720};
    /** Screen slot for this document. Fill covers the host passed to Draw. */
    UiPlace place = UiPlace::Fill;
    /** Used when place is Free. Reference pixels, scaled with the host. */
    Vec2 placePosition{0, 0};
    /** Slot size in reference pixels. Zero on an axis uses referenceSize. */
    Vec2 placeSize{0, 0};
    std::vector<UiElement> elements;

    /** Throws for duplicate ids, missing/non-panel parents, cycles or invalid values. */
    void Validate() const;
    /** Returns deterministic version 2 text after validating this document. Version 1 files still parse. */
    [[nodiscard]] std::string Serialize() const;
    /** Validates before replacing this document; a failed parse leaves it unchanged. */
    void Parse(const std::string& text);
    /** Reads at most 4 MiB and transactionally parses the UTF-8 document. */
    void Load(const std::filesystem::path& path);
    /** Atomically replaces the destination after validating and flushing a sibling file. */
    void Save(const std::filesystem::path& path) const;
    /**
     * Rectangle this document occupies inside the host. Fill returns the host.
     * Top and bottom span the width; left and right span the height. Center and
     * Free use the slot size. A zero slot axis falls back to the reference size.
     */
    [[nodiscard]] UiDrawDesc Region(const UiDrawDesc& host) const;
    /**
     * Controls are laid out inside Region(desc). Uniform scale is
     * min(region / reference). Free controls use
     * parent.position + anchor * (parent.size - scaled size) + scaled position.
     * Other places dock to the parent. Visibility, enabled state and clipping
     * are inherited.
     */
    [[nodiscard]] std::vector<UiElementLayout> ResolveLayout(const UiDrawDesc& desc) const;
    /** Renders controls, updates their values and returns events; preview mode consumes no input. */
    [[nodiscard]] std::vector<UiEvent> Draw(const UiDrawDesc& desc);
};

/**
 * Replaces the document when CONCORD_UI names a .yu file.
 *
 * The editor sets that variable for Play and Preview so an interface edit
 * opens from disk instead of the copy compiled into the game.
 *
 * @return True when the variable was set and the file loaded.
 */
CENGINE_API bool LoadUiOverride(UiDocument& document);
}
#endif
