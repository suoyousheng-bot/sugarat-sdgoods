cmake_minimum_required(VERSION 3.16)
project(pet_host C)
get_filename_component(FIRMWARE "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
file(GLOB_RECURSE LVGL_SRC "${FIRMWARE}/components/lvgl/src/*.c")
file(GLOB_RECURSE WATCH_SRC "${FIRMWARE}/components/sugarat_watch/src/*.c")
file(GLOB EPET_SRC "${FIRMWARE}/components/epet_core/*.c")
add_library(pet-epet STATIC ${EPET_SRC})
target_include_directories(pet-epet PUBLIC "${FIRMWARE}/components/epet_core")
target_compile_options(pet-epet PRIVATE -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
add_executable(pet-host test.c ${LVGL_SRC} ${WATCH_SRC}
    "${FIRMWARE}/main/apps/ui_helloapp.c" "${FIRMWARE}/main/pet/pet_model.c" "${FIRMWARE}/main/pet/pet_character.c"
    "${FIRMWARE}/main/pet/pet_save.c" "${FIRMWARE}/main/pet/pet_storage.c" "${FIRMWARE}/main/pet/pet_view.c")
target_include_directories(pet-host PRIVATE . "${FIRMWARE}/components/lvgl" "${FIRMWARE}/components/epet_core"
    "${FIRMWARE}/main/pet" "${FIRMWARE}/main/apps" "${FIRMWARE}/components/sugarat_watch/include")
target_compile_definitions(pet-host PRIVATE LV_CONF_INCLUDE_SIMPLE _DARWIN_C_SOURCE _POSIX_C_SOURCE=200809L)
target_compile_options(pet-host PRIVATE -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(pet-host PRIVATE -fsanitize=address,undefined)
target_link_libraries(pet-host PRIVATE m pet-epet)

if(APPLE AND EXISTS "/opt/homebrew/opt/openssl@3")
    set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl@3" CACHE PATH "OpenSSL prefix for native protocol checks")
endif()
find_package(OpenSSL 3 REQUIRED)
add_executable(pet-family-tests family_test.c
    "${FIRMWARE}/main/pet/pet_model.c" "${FIRMWARE}/main/pet/pet_character.c"
    "${FIRMWARE}/main/pet/pet_crypto.c" "${FIRMWARE}/main/pet/pet_family.c" "${FIRMWARE}/main/pet/pet_family_record.c")
target_include_directories(pet-family-tests PRIVATE "${FIRMWARE}/components/epet_core" "${FIRMWARE}/main/pet")
target_compile_options(pet-family-tests PRIVATE -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(pet-family-tests PRIVATE -fsanitize=address,undefined)
target_link_libraries(pet-family-tests PRIVATE m pet-epet OpenSSL::Crypto)

add_executable(pet-social-tests social_test.c
    "${FIRMWARE}/main/pet/pet_model.c" "${FIRMWARE}/main/pet/pet_character.c"
    "${FIRMWARE}/main/pet/pet_crypto.c" "${FIRMWARE}/main/pet/pet_family.c" "${FIRMWARE}/main/pet/pet_family_record.c"
    "${FIRMWARE}/main/pet/pet_social.c" "${FIRMWARE}/main/pet/pet_family_store.c")
target_include_directories(pet-social-tests PRIVATE "${FIRMWARE}/components/epet_core" "${FIRMWARE}/main/pet")
target_compile_options(pet-social-tests PRIVATE -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(pet-social-tests PRIVATE -fsanitize=address,undefined)
target_link_libraries(pet-social-tests PRIVATE m pet-epet OpenSSL::Crypto)

target_sources(pet-host PRIVATE radio_stub.c
    "${FIRMWARE}/main/pet/pet_crypto.c" "${FIRMWARE}/main/pet/pet_family.c" "${FIRMWARE}/main/pet/pet_family_record.c"
    "${FIRMWARE}/main/pet/pet_social.c" "${FIRMWARE}/main/pet/pet_family_store.c"
    "${FIRMWARE}/main/pet/pet_animation.c" "${FIRMWARE}/main/pet/pet_art.c")
target_link_libraries(pet-host PRIVATE OpenSSL::Crypto)

target_sources(pet-host PRIVATE "${FIRMWARE}/main/pet/pet_controls.c"
    "${FIRMWARE}/main/pet/pet_gestures.c"
    "${FIRMWARE}/components/sdgoods_board/src/sdgoods_tap.c"
    "${FIRMWARE}/components/sdgoods_board/src/sdgoods_swipe_up.c"
    "${FIRMWARE}/components/sdgoods_board/src/sdgoods_swipe_back.c"
    "${FIRMWARE}/components/sdgoods_board/src/sdgoods_gesture.c")
target_include_directories(pet-host PRIVATE "${FIRMWARE}/components/sdgoods_board/include")

target_sources(pet-host PRIVATE "${FIRMWARE}/main/pet/pet_family_nvs.c")
add_executable(pet-nvs-tests nvs_family_test.c
    "${FIRMWARE}/main/pet/pet_model.c" "${FIRMWARE}/main/pet/pet_character.c" "${FIRMWARE}/main/pet/pet_crypto.c"
    "${FIRMWARE}/main/pet/pet_family.c" "${FIRMWARE}/main/pet/pet_family_record.c"
    "${FIRMWARE}/main/pet/pet_family_store.c" "${FIRMWARE}/main/pet/pet_family_nvs.c")
target_include_directories(pet-nvs-tests PRIVATE . "${FIRMWARE}/components/epet_core" "${FIRMWARE}/main/pet")
target_compile_options(pet-nvs-tests PRIVATE -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(pet-nvs-tests PRIVATE -fsanitize=address,undefined)
target_link_libraries(pet-nvs-tests PRIVATE m pet-epet OpenSSL::Crypto)
