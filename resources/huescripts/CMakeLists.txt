project(huescripts)

set(SCRIPT_FILES
    audiobands.js
    audiobandsmatrix.js
    audiobladepower.js
    audiobleep.js
    audiobpmbar.js
    audioconcentric.js
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
    audiodigitalrain.js
    audiodjlight.js
    audioenergy.js
    audioenergy2.js
    audioequalizer.js
    audioequalizer2d.js
    audiofilter.js
    audiofire.js
    audioflame.js
    audiofireworks.js
    audioflowfield.js
    audiogameoflife.js
    audioglitch.js
    audioglitch2.js
    audiogravimeter.js
    audiohierarchy.js
    audiohueshift.js
    audiolava.js
    audiomagnitude.js
    audiomarching.js
    audiomelt.js
    audiomeltsparkle.js
    audiomulticolorbar.js
    audionoise.js
    audioplasma.js
    audiopower.js
    audiopitchspectrum.js
    audiopuddles.js
    audioreaction.js
    audioreactor.js
    audioscan.js
    audioscanflare.js
    audioscanmulti.js
    audioshockwave.js
    audioshot.js
    audiosoap.js
    audiosmoke.js
    audiospectralblocks.js
    audiospotlight.js
    audiospectrum.js
    audiosplittower.js
    audiostrobe.js
    audiotunnel.js
    audiovumeter.js
    audiovortex.js
    audiowater.js
    audiowaterfall.js
    huefade.js
    huegradient.js
    huemetro.js
    huepixels.js
    huerainbow.js
    huerandomflash.js
    huesinglecolor.js
    hsvutil.js
)

# The engine tests resolve INTERNAL_SCRIPTDIR / INTERNAL_HUESCRIPTDIR relative to
# their own build directory ("../../../resources/huescripts/"), so the scripts have to
# exist inside the build tree or rgbscript, rgbmatrix, rgbalgorithm and huematrix
# all fail in initTestCase. Nothing else populated it, so a fresh clone could not
# run the suite. rgbscript_test also opens this CMakeLists.txt to check every
# script is listed for delivery, so copy the file itself too -- and writable,
# because it opens it ReadWrite.
set(SCRIPTS_BUILD_DIR "${CMAKE_BINARY_DIR}/resources/huescripts")
file(MAKE_DIRECTORY ${SCRIPTS_BUILD_DIR})
file(COPY ${SCRIPT_FILES} CMakeLists.txt
     DESTINATION ${SCRIPTS_BUILD_DIR}
     FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

# Only perform the copy if we're building for Android
if(ANDROID)
    set(SCRIPTS_ASSETS_DIR "${QT_ANDROID_PACKAGE_SOURCE_DIR}/${INSTALLROOT}/${HUESCRIPTDIR}")
    file(MAKE_DIRECTORY ${SCRIPTS_ASSETS_DIR})
    foreach(SCRIPT_FILE IN LISTS SCRIPT_FILES)
        file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${SCRIPT_FILE} DESTINATION ${SCRIPTS_ASSETS_DIR})
    endforeach()
endif()

install(FILES ${SCRIPT_FILES} DESTINATION ${INSTALLROOT}/${HUESCRIPTDIR})
