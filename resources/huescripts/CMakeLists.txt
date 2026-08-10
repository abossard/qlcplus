project(huescripts)

set(SCRIPT_FILES
    audioaurora.js
    audiobarcode.js
    audiobasslaser.js
    audiobeatcolors.js
    audioblocks.js
    audioblurz.js
    audiobuildup.js
    audiocellular.js
    audiochaser.js
    audiocrawler.js
    audiodjlight.js
    audioenergy.js
    audioenergy2.js
    audioequalizer.js
    audiofire.js
    audiofireworks.js
    audioflowfield.js
    audioglitch.js
    audioglitch2.js
    audiogravimeter.js
    audiohueshift.js
    audiolava.js
    audiomelt.js
    audiomeltsparkle.js
    audioplasma.js
    audiopower.js
    audiopuddles.js
    audioreaction.js
    audioreactor.js
    audioscan.js
    audioscanflare.js
    audioscanmulti.js
    audioshockwave.js
    audioshot.js
    audiosoap.js
    audiospectrum.js
    audiosplittower.js
    audiostrobe.js
    audiotunnel.js
    audiovortex.js
    audiowater.js
    hsvutil.js
)

# Only perform the copy if we're building for Android
if(ANDROID)
    set(SCRIPTS_ASSETS_DIR "${QT_ANDROID_PACKAGE_SOURCE_DIR}/${INSTALLROOT}/${HUESCRIPTDIR}")
    file(MAKE_DIRECTORY ${SCRIPTS_ASSETS_DIR})
    foreach(SCRIPT_FILE IN LISTS SCRIPT_FILES)
        file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${SCRIPT_FILE} DESTINATION ${SCRIPTS_ASSETS_DIR})
    endforeach()
endif()

install(FILES ${SCRIPT_FILES} DESTINATION ${INSTALLROOT}/${HUESCRIPTDIR})
