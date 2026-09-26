// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "Concord/CUiDocument.h"

#include <windows.h>

#include <atomic>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace Concord {
namespace {
constexpr usize MaxDocumentBytes = 4 * 1024 * 1024;
std::atomic<u64> temporarySequence{0};
}

std::string UiDocument::Serialize() const
{
    Validate();
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(9) << "CONCORD_UI 1\n" << referenceSize.x << ' ' << referenceSize.y << ' ' << elements.size() << '\n';
    for (const auto& element : elements) {
        out << static_cast<int>(element.kind) << ' ' << static_cast<int>(element.action) << ' '
            << std::quoted(element.id) << ' ' << std::quoted(element.parent) << ' '
            << std::quoted(element.text) << ' ' << std::quoted(element.input) << ' '
            << element.position.x << ' ' << element.position.y << ' ' << element.size.x << ' ' << element.size.y << ' '
            << element.anchor.x << ' ' << element.anchor.y << ' ' << element.color << ' ' << element.background << ' '
            << element.rounding << ' ' << element.fontScale << ' ' << element.value << ' '
            << element.visible << ' ' << element.enabled << '\n';
    }
    std::string text = out.str();
    if (text.size() > MaxDocumentBytes) throw std::runtime_error("UI document exceeds 4 MiB");
    return text;
}

void UiDocument::Parse(const std::string& text)
{
    if (text.size() > MaxDocumentBytes) throw std::runtime_error("UI document exceeds 4 MiB");
    std::istringstream input(text);
    input.imbue(std::locale::classic());
    UiDocument parsed;
    std::string magic;
    int version = 0;
    usize count = 0;
    if (!(input >> magic >> version >> parsed.referenceSize.x >> parsed.referenceSize.y >> count) ||
        magic != "CONCORD_UI" || version != 1 || count > 2048)
        throw std::runtime_error("Unsupported or corrupt .yu document header");
    parsed.elements.reserve(count);
    for (usize index = 0; index < count; ++index) {
        UiElement element;
        int kind = 0, action = 0;
        if (!(input >> kind >> action >> std::quoted(element.id) >> std::quoted(element.parent) >>
            std::quoted(element.text) >> std::quoted(element.input) >>
            element.position.x >> element.position.y >> element.size.x >> element.size.y >>
            element.anchor.x >> element.anchor.y >> element.color >> element.background >>
            element.rounding >> element.fontScale >> element.value >> element.visible >> element.enabled))
            throw std::runtime_error("Invalid .yu element at index " + std::to_string(index));
        element.kind = static_cast<UiElementKind>(kind);
        element.action = static_cast<UiAction>(action);
        parsed.elements.push_back(std::move(element));
    }
    input >> std::ws;
    if (!input.eof()) throw std::runtime_error("Unexpected data after .yu elements");
    parsed.Validate();
    *this = std::move(parsed);
}

void UiDocument::Load(const std::filesystem::path& path)
{
    if (std::filesystem::file_size(path) > MaxDocumentBytes) throw std::runtime_error("UI document exceeds 4 MiB");
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open .yu document");
    std::string text(MaxDocumentBytes + 1, '\0');
    file.read(text.data(), static_cast<std::streamsize>(text.size()));
    text.resize(static_cast<usize>(file.gcount()));
    if (file.bad()) throw std::runtime_error("Cannot read .yu document");
    Parse(text);
}

void UiDocument::Save(const std::filesystem::path& path) const
{
    const std::string text = Serialize();
    auto temporary = path;
    temporary += L".tmp." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(temporarySequence.fetch_add(1));
    const HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create temporary .yu document");
    DWORD written = 0;
    const bool complete = WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) &&
        written == text.size() && FlushFileBuffers(file);
    CloseHandle(file);
    if (!complete || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str());
        throw std::runtime_error("Cannot atomically save .yu document");
    }
}
}
