# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_executable(concord_code_editor_tests tests/CodeEditorTests.cpp)
target_compile_features(concord_code_editor_tests PRIVATE cxx_std_23)
target_include_directories(concord_code_editor_tests PRIVATE ${CONCORD_3RD_DIR}/SDL3)
target_link_libraries(concord_code_editor_tests PRIVATE concord::runtime SDL3::SDL3)
set_target_properties(concord_code_editor_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
add_custom_command(TARGET concord_code_editor_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CONCORD_LIB_DIR}/SDL3.dll" "$<TARGET_FILE_DIR:concord_code_editor_tests>")
add_test(NAME concord_code_editor_tests COMMAND concord_code_editor_tests)
set_tests_properties(concord_code_editor_tests PROPERTIES TIMEOUT 20)

add_executable(concord_ui_toolkit_tests tests/UiToolkitTests.cpp)
target_link_libraries(concord_ui_toolkit_tests PRIVATE concord::runtime)
set_target_properties(concord_ui_toolkit_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
add_test(NAME concord_ui_toolkit_tests COMMAND concord_ui_toolkit_tests)
set_tests_properties(concord_ui_toolkit_tests PROPERTIES TIMEOUT 60)

add_executable(concord_ui_document_tests tests/UiDocumentTests.cpp)
target_link_libraries(concord_ui_document_tests PRIVATE concord::runtime)
set_target_properties(concord_ui_document_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
add_test(NAME concord_ui_document_tests COMMAND concord_ui_document_tests)
set_tests_properties(concord_ui_document_tests PROPERTIES TIMEOUT 30)
