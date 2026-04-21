cmake_minimum_required(VERSION 3.14)

file(READ "src/okjversion.h" vercontents)
string(REGEX MATCH "OKJ_VERSION_MAJOR ([0-9]*)" _ ${vercontents})
set(ver_major ${CMAKE_MATCH_1})
string(REGEX MATCH "OKJ_VERSION_MINOR ([0-9]*)" _ ${vercontents})
set(ver_minor ${CMAKE_MATCH_1})
string(REGEX MATCH "OKJ_VERSION_BUILD ([0-9]*)" _ ${vercontents})
set(ver_build ${CMAKE_MATCH_1})
set(sem_ver "${ver_major}.${ver_minor}.${ver_build}")

message(STATUS "Building Auto-KJ ${sem_ver}")

message("Building Auto-KJ ${sem_ver}")

project(auto-kj
        VERSION ${sem_ver}
        LANGUAGES CXX C
        )

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt5 COMPONENTS Widgets REQUIRED)

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Gui Sql Network WebSockets Widgets Concurrent Svg PrintSupport WebChannel WebEngineWidgets REQUIRED)

include(FetchContent)
FetchContent_Declare(
  qrencode
  GIT_REPOSITORY https://github.com/fukuchi/libqrencode.git
  GIT_TAG master
)
set(WITHOUT_ENCODE_PROG ON CACHE BOOL "" FORCE) # Don't build CLI tool
set(WITHOUT_TOOLS ON CACHE BOOL "" FORCE)       # Newer qrencode variable name
set(WITH_TOOLS OFF CACHE BOOL "" FORCE)
set(WITH_TESTS OFF CACHE BOOL "" FORCE) # Don't build tests
FetchContent_MakeAvailable(qrencode)
set_target_properties(qrencode PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)

include_directories(
        src/
        ${qrencode_SOURCE_DIR}
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    find_package(PkgConfig)
    pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-app-1.0 gstreamer-audio-1.0 gstreamer-pbutils-1.0 gstreamer-controller-1.0 gstreamer-video-1.0)
    include_directories(
            ${GSTREAMER_INCLUDE_DIRS}
            ${GLIB_LIBRARY_DIRS}
    )
    link_directories(
            ${GSTREAMER_LIBRARY_DIRS}
            ${GLIB_INCLUDE_DIRS}
    )
    pkg_check_modules(TAGLIB IMPORTED_TARGET taglib)
    if (TAGLIB_FOUND)
        message(STATUS "Using OS installed taglib")
        include_directories(
                ${TAGLIB_INCLUDE_DIRS}
        )
        link_directories(
                ${TAGLIB_LIBRARY_DIRS}
        )
        set(EXTERNAL_TAGLIB True)
    else ()
        set(STATIC_TAGLIB True)
    endif ()
    if (NOT DEFINED SPDLOG_USE_CPM AND NOT DEFINED SPDLOG_USE_BUNDLED)
        pkg_check_modules(SPDLOG IMPORTED_TARGET spdlog)
        if (SPDLOG_FOUND)
            message(STATUS "Using OS installed spdlog")
            add_definitions(-DSPDLOG_FMT_EXTERNAL)
            link_directories(${SPDLOG_LIBRARY_DIRS})
            include_directories(${SPDLOG_INCLUDE_DIRS})
            set(EXTERNAL_SPDLOG True)
            #        include_directories(
            #                ${SPDLOG_INCLUDE_DIRS}
            #        )
        endif ()
    else ()
        message("SPDLOG_USE_CPM or SPDLOG_USE_BUNDLED defined, ignoring OS packages")
    endif ()
endif ()

if (NOT EXTERNAL_SPDLOG)
    if (CMAKE_VERSION VERSION_LESS 3.14 OR DEFINED SPDLOG_USE_BUNDLED)
        message(STATUS "No spdlog install found, SPDLOG_USE_BUNDLED define, or SPDLOG_USE_CPM defined and cmake too old for CPM, trying git submodule")
        add_subdirectory(
                src/3rdparty/spdlog
        )
        include_directories(
                src/3rdparty/spdlog/include
        )
    else ()
        message(STATUS "No spdlog install found or CPM spdlog forced, pulling it in via CPM")
        include(cmake/CPM.cmake)
        CPMAddPackage("gh:gabime/spdlog@1.12.0")
    endif ()
endif ()


if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    message("macOS detected")
    set(STATIC_TAGLIB True)

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-app-1.0 gstreamer-audio-1.0 gstreamer-pbutils-1.0 gstreamer-controller-1.0 gstreamer-video-1.0)

    include_directories(
            ${GSTREAMER_INCLUDE_DIRS}
    )
    link_directories(
            ${GSTREAMER_LIBRARY_DIRS}
    )
    if (DEFINED BUNDLE_GSTREAMER)
        message("Will bundle GStreamer into the app bundle")
        add_compile_definitions(
                MAC_OVERRIDE_GST
        )
    endif ()
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(STATIC_TAGLIB True)
    set(WINDOWS_ICON_RC "src/appicon.rc")
    # Prevent C++ compile flags from being passed to rc.exe
    set(CMAKE_RC_FLAGS "/nologo")
    if (NOT DEFINED GST_BASE_PATH)
        if (CMAKE_SIZEOF_VOID_P EQUAL 4)
            if (EXISTS "C:/Program Files/gstreamer/1.0/msvc_x86")
                set(GST_BASE_PATH "C:/Program Files/gstreamer/1.0/msvc_x86")
            else()
                set(GST_BASE_PATH "C:/Program Files/gstreamer/1.0/mingw_x86")
            endif()
        elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
            if (EXISTS "C:/Program Files/gstreamer/1.0/msvc_x86_64")
                set(GST_BASE_PATH "C:/Program Files/gstreamer/1.0/msvc_x86_64")
            else()
                set(GST_BASE_PATH "C:/Program Files/gstreamer/1.0/mingw_x86_64")
            endif()
        endif()
    endif()
    string(REPLACE "/" "\\" GST_BASE_PATH_NATIVE "${GST_BASE_PATH}")
    message("Using GST_BASE_PATH: ${GST_BASE_PATH}")
    message("Using GST_BASE_PATH_NATIVE: ${GST_BASE_PATH_NATIVE}")
    include_directories(
            ${GST_BASE_PATH}/include/gstreamer-1.0
            ${GST_BASE_PATH}/include/glib-2.0
            ${GST_BASE_PATH}/lib/glib-2.0/include
            ${GST_BASE_PATH}/include/glib-2.0/gobject
    )
    link_directories(
            "${GST_BASE_PATH}/lib"
    )
    add_compile_definitions(
            MINIZ_NO_TIME
    )

    link_libraries(
            gstreamer-1.0
            glib-2.0
            gobject-2.0
            gstbase-1.0
            gstapp-1.0
            gstaudio-1.0
            gstpbutils-1.0
            gstcontroller-1.0
            gstvideo-1.0
            winmm
    )

endif ()

option(AUTOKJ_ENABLE_QT_TESTS "Build Qt-based desktop unit tests" OFF)
if (AUTOKJ_ENABLE_QT_TESTS)
    enable_testing()
    find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Test REQUIRED)
    add_executable(autokj-client-mock-tests
            tests/cpp/test_autokjserverclientmock.cpp
            src/testing/autokjserverclientmock.cpp
            src/testing/autokjserverclientmock.h
            src/autokjserverclient.h
    )
    target_link_libraries(autokj-client-mock-tests
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Test
    )
    add_test(NAME autokj-client-mock-tests COMMAND autokj-client-mock-tests)

    add_executable(autokj-client-integration-smoke-tests
            tests/cpp/test_autokj_client_integration_smoke.cpp
            src/testing/autokjserverclientmock.cpp
            src/testing/autokjserverclientmock.h
            src/autokjserverclient.h
    )
    target_link_libraries(autokj-client-integration-smoke-tests
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Test
    )
    add_test(NAME autokj-client-integration-smoke-tests COMMAND autokj-client-integration-smoke-tests)
endif ()

if (STATIC_TAGLIB)
    message("Building using included taglib")
    add_compile_definitions(
            TAGLIB_STATIC
    )
    include_directories(
            src/taglib
            src/taglib/toolkit
            src/taglib/mpeg/id3v2
            src/taglib/mpeg/id3v2/frames
            src/taglib/mpeg/id3v1
            src/taglib/mod
            src/taglib/ogg
            src/taglib/ogg/flac
            src/taglib/ogg/opus
            src/taglib/ogg/speex
            src/taglib/ogg/vorbis
            src/taglib/flac
            src/taglib/ape
            src/taglib/riff
            src/taglib/asf
            src/taglib/mpeg
            src/taglib/riff
            src/taglib/riff/aiff
            src/taglib/riff/wav
            src/taglib/it
            src/taglib/mp4
            src/taglib/mpc
            src/taglib/s3m
            src/taglib/trueaudio
            src/taglib/wavpack
            src/taglib/xm
    )
    set(TAGLIB_SOURCES
            src/taglib/ape/apefile.cpp
            src/taglib/ape/apefooter.cpp
            src/taglib/ape/apeitem.cpp
            src/taglib/ape/apeproperties.cpp
            src/taglib/ape/apetag.cpp
            src/taglib/asf/asfattribute.cpp
            src/taglib/asf/asffile.cpp
            src/taglib/asf/asfpicture.cpp
            src/taglib/asf/asfproperties.cpp
            src/taglib/asf/asftag.cpp
            src/taglib/flac/flacfile.cpp
            src/taglib/flac/flacmetadatablock.cpp
            src/taglib/flac/flacpicture.cpp
            src/taglib/flac/flacproperties.cpp
            src/taglib/flac/flacunknownmetadatablock.cpp
            src/taglib/it/itfile.cpp
            src/taglib/it/itproperties.cpp
            src/taglib/mod/modfile.cpp
            src/taglib/mod/modfilebase.cpp
            src/taglib/mod/modproperties.cpp
            src/taglib/mod/modtag.cpp
            src/taglib/mp4/mp4atom.cpp
            src/taglib/mp4/mp4coverart.cpp
            src/taglib/mp4/mp4file.cpp
            src/taglib/mp4/mp4item.cpp
            src/taglib/mp4/mp4properties.cpp
            src/taglib/mp4/mp4tag.cpp
            src/taglib/mpc/mpcfile.cpp
            src/taglib/mpc/mpcproperties.cpp
            src/taglib/mpeg/id3v1/id3v1genres.cpp
            src/taglib/mpeg/id3v1/id3v1tag.cpp
            src/taglib/mpeg/id3v2/frames/attachedpictureframe.cpp
            src/taglib/mpeg/id3v2/frames/chapterframe.cpp
            src/taglib/mpeg/id3v2/frames/commentsframe.cpp
            src/taglib/mpeg/id3v2/frames/eventtimingcodesframe.cpp
            src/taglib/mpeg/id3v2/frames/generalencapsulatedobjectframe.cpp
            src/taglib/mpeg/id3v2/frames/ownershipframe.cpp
            src/taglib/mpeg/id3v2/frames/podcastframe.cpp
            src/taglib/mpeg/id3v2/frames/popularimeterframe.cpp
            src/taglib/mpeg/id3v2/frames/privateframe.cpp
            src/taglib/mpeg/id3v2/frames/relativevolumeframe.cpp
            src/taglib/mpeg/id3v2/frames/synchronizedlyricsframe.cpp
            src/taglib/mpeg/id3v2/frames/tableofcontentsframe.cpp
            src/taglib/mpeg/id3v2/frames/textidentificationframe.cpp
            src/taglib/mpeg/id3v2/frames/uniquefileidentifierframe.cpp
            src/taglib/mpeg/id3v2/frames/unknownframe.cpp
            src/taglib/mpeg/id3v2/frames/unsynchronizedlyricsframe.cpp
            src/taglib/mpeg/id3v2/frames/urllinkframe.cpp
            src/taglib/mpeg/id3v2/id3v2extendedheader.cpp
            src/taglib/mpeg/id3v2/id3v2footer.cpp
            src/taglib/mpeg/id3v2/id3v2frame.cpp
            src/taglib/mpeg/id3v2/id3v2framefactory.cpp
            src/taglib/mpeg/id3v2/id3v2header.cpp
            src/taglib/mpeg/id3v2/id3v2synchdata.cpp
            src/taglib/mpeg/id3v2/id3v2tag.cpp
            src/taglib/mpeg/mpegfile.cpp
            src/taglib/mpeg/mpegheader.cpp
            src/taglib/mpeg/mpegproperties.cpp
            src/taglib/mpeg/xingheader.cpp
            src/taglib/ogg/flac/oggflacfile.cpp
            src/taglib/ogg/opus/opusfile.cpp
            src/taglib/ogg/opus/opusproperties.cpp
            src/taglib/ogg/speex/speexfile.cpp
            src/taglib/ogg/speex/speexproperties.cpp
            src/taglib/ogg/vorbis/vorbisfile.cpp
            src/taglib/ogg/vorbis/vorbisproperties.cpp
            src/taglib/ogg/oggfile.cpp
            src/taglib/ogg/oggpage.cpp
            src/taglib/ogg/oggpageheader.cpp
            src/taglib/ogg/xiphcomment.cpp
            src/taglib/riff/aiff/aifffile.cpp
            src/taglib/riff/aiff/aiffproperties.cpp
            src/taglib/riff/wav/infotag.cpp
            src/taglib/riff/wav/wavfile.cpp
            src/taglib/riff/wav/wavproperties.cpp
            src/taglib/riff/rifffile.cpp
            src/taglib/s3m/s3mfile.cpp
            src/taglib/s3m/s3mproperties.cpp
            src/taglib/toolkit/tbytevector.cpp
            src/taglib/toolkit/tbytevectorlist.cpp
            src/taglib/toolkit/tbytevectorstream.cpp
            src/taglib/toolkit/tdebug.cpp
            src/taglib/toolkit/tdebuglistener.cpp
            src/taglib/toolkit/tfile.cpp
            src/taglib/toolkit/tfilestream.cpp
            src/taglib/toolkit/tiostream.cpp
            src/taglib/toolkit/tpropertymap.cpp
            src/taglib/toolkit/trefcounter.cpp
            src/taglib/toolkit/tstring.cpp
            src/taglib/toolkit/tstringlist.cpp
            src/taglib/toolkit/tzlib.cpp
            src/taglib/toolkit/unicode.cpp
            src/taglib/trueaudio/trueaudiofile.cpp
            src/taglib/trueaudio/trueaudioproperties.cpp
            src/taglib/wavpack/wavpackfile.cpp
            src/taglib/wavpack/wavpackproperties.cpp
            src/taglib/xm/xmfile.cpp
            src/taglib/xm/xmproperties.cpp
            src/taglib/audioproperties.cpp
            src/taglib/fileref.cpp
            src/taglib/tag.cpp
            src/taglib/tagunion.cpp
            src/taglib/tagutils.cpp
            src/taglib/ape/apefile.h
            src/taglib/ape/apefooter.h
            src/taglib/ape/apeitem.h
            src/taglib/ape/apeproperties.h
            src/taglib/ape/apetag.h
            src/taglib/asf/asfattribute.h
            src/taglib/asf/asffile.h
            src/taglib/asf/asfpicture.h
            src/taglib/asf/asfproperties.h
            src/taglib/asf/asftag.h
            src/taglib/asf/asfutils.h
            src/taglib/flac/flacfile.h
            src/taglib/flac/flacmetadatablock.h
            src/taglib/flac/flacpicture.h
            src/taglib/flac/flacproperties.h
            src/taglib/flac/flacunknownmetadatablock.h
            src/taglib/it/itfile.h
            src/taglib/it/itproperties.h
            src/taglib/mod/modfile.h
            src/taglib/mod/modfilebase.h
            src/taglib/mod/modfileprivate.h
            src/taglib/mod/modproperties.h
            src/taglib/mod/modtag.h
            src/taglib/mp4/mp4atom.h
            src/taglib/mp4/mp4coverart.h
            src/taglib/mp4/mp4file.h
            src/taglib/mp4/mp4item.h
            src/taglib/mp4/mp4properties.h
            src/taglib/mp4/mp4tag.h
            src/taglib/mpc/mpcfile.h
            src/taglib/mpc/mpcproperties.h
            src/taglib/mpeg/id3v1/id3v1genres.h
            src/taglib/mpeg/id3v1/id3v1tag.h
            src/taglib/mpeg/id3v2/frames/attachedpictureframe.h
            src/taglib/mpeg/id3v2/frames/chapterframe.h
            src/taglib/mpeg/id3v2/frames/commentsframe.h
            src/taglib/mpeg/id3v2/frames/eventtimingcodesframe.h
            src/taglib/mpeg/id3v2/frames/generalencapsulatedobjectframe.h
            src/taglib/mpeg/id3v2/frames/ownershipframe.h
            src/taglib/mpeg/id3v2/frames/podcastframe.h
            src/taglib/mpeg/id3v2/frames/popularimeterframe.h
            src/taglib/mpeg/id3v2/frames/privateframe.h
            src/taglib/mpeg/id3v2/frames/relativevolumeframe.h
            src/taglib/mpeg/id3v2/frames/synchronizedlyricsframe.h
            src/taglib/mpeg/id3v2/frames/tableofcontentsframe.h
            src/taglib/mpeg/id3v2/frames/textidentificationframe.h
            src/taglib/mpeg/id3v2/frames/uniquefileidentifierframe.h
            src/taglib/mpeg/id3v2/frames/unknownframe.h
            src/taglib/mpeg/id3v2/frames/unsynchronizedlyricsframe.h
            src/taglib/mpeg/id3v2/frames/urllinkframe.h
            src/taglib/mpeg/id3v2/id3v2extendedheader.h
            src/taglib/mpeg/id3v2/id3v2footer.h
            src/taglib/mpeg/id3v2/id3v2frame.h
            src/taglib/mpeg/id3v2/id3v2framefactory.h
            src/taglib/mpeg/id3v2/id3v2header.h
            src/taglib/mpeg/id3v2/id3v2synchdata.h
            src/taglib/mpeg/id3v2/id3v2tag.h
            src/taglib/mpeg/mpegfile.h
            src/taglib/mpeg/mpegheader.h
            src/taglib/mpeg/mpegproperties.h
            src/taglib/mpeg/mpegutils.h
            src/taglib/mpeg/xingheader.h
            src/taglib/ogg/flac/oggflacfile.h
            src/taglib/ogg/opus/opusfile.h
            src/taglib/ogg/opus/opusproperties.h
            src/taglib/ogg/speex/speexfile.h
            src/taglib/ogg/speex/speexproperties.h
            src/taglib/ogg/vorbis/vorbisfile.h
            src/taglib/ogg/vorbis/vorbisproperties.h
            src/taglib/ogg/oggfile.h
            src/taglib/ogg/oggpage.h
            src/taglib/ogg/oggpageheader.h
            src/taglib/ogg/xiphcomment.h
            src/taglib/riff/aiff/aifffile.h
            src/taglib/riff/aiff/aiffproperties.h
            src/taglib/riff/wav/infotag.h
            src/taglib/riff/wav/wavfile.h
            src/taglib/riff/wav/wavproperties.h
            src/taglib/riff/rifffile.h
            src/taglib/riff/riffutils.h
            src/taglib/s3m/s3mfile.h
            src/taglib/s3m/s3mproperties.h
            src/taglib/toolkit/taglib.h
            src/taglib/toolkit/tbytevector.h
            src/taglib/toolkit/tbytevectorlist.h
            src/taglib/toolkit/tbytevectorstream.h
            src/taglib/toolkit/tdebug.h
            src/taglib/toolkit/tdebuglistener.h
            src/taglib/toolkit/tfile.h
            src/taglib/toolkit/tfilestream.h
            src/taglib/toolkit/tiostream.h
            src/taglib/toolkit/tlist.h
            src/taglib/toolkit/tmap.h
            src/taglib/toolkit/tpropertymap.h
            src/taglib/toolkit/trefcounter.h
            src/taglib/toolkit/tstring.h
            src/taglib/toolkit/tstringlist.h
            src/taglib/toolkit/tutils.h
            src/taglib/toolkit/tzlib.h
            src/taglib/toolkit/unicode.h
            src/taglib/trueaudio/trueaudiofile.h
            src/taglib/trueaudio/trueaudioproperties.h
            src/taglib/wavpack/wavpackfile.h
            src/taglib/wavpack/wavpackproperties.h
            src/taglib/xm/xmfile.h
            src/taglib/xm/xmproperties.h
            src/taglib/audioproperties.h
            src/taglib/fileref.h
            src/taglib/tag.h
            src/taglib/taglib_export.h
            src/taglib/tagunion.h
            src/taglib/tagutils.h
            )
endif ()

set(SOURCE_FILES
        ${TAGLIB_SOURCES}
        src/miniz/miniz.h
        src/miniz/miniz.c
        src/main.cpp
        src/dlgaddsong.cpp
        src/mediabackend.cpp
        src/mzarchive.cpp
        src/okjutil.h
        src/okjmigration.h
        src/okjtypes.cpp
        src/dlgvideopreview.cpp
        src/mainwindow.cpp
        src/dbupdater.cpp
        src/directorymonitor.cpp
        src/dlgkeychange.cpp
        src/dlgdatabase.cpp
        src/dlgaddvenue.cpp
        src/dlgrequests.cpp
        src/dlgregularexport.cpp
        src/dlgregularimport.cpp
        src/dlgregularsingers.cpp
        src/dlgregister.cpp
        src/dlgsettings.cpp
        src/dlgcdg.cpp
        src/okarchive.cpp
        src/models/tablemodelbreaksongs.cpp
        src/models/tablemodelcustomnamingpatterns.cpp
        src/models/tablemodelhistorysingers.cpp
        src/models/tablemodelhistorysongs.cpp
        src/models/tablemodelkaraokesongs.cpp
        src/models/tablemodelkaraokesourcedirs.cpp
        src/models/tablemodelplaylistsongs.cpp
        src/models/tablemodelqueuesongs.cpp
        src/models/tablemodelrequests.cpp
        src/models/tablemodelrotation.cpp

        src/tagreader.cpp
        src/bmdbupdatethread.cpp
        src/settings.cpp
        src/bmdbdialog.cpp
        src/dlgcustompatterns.cpp
        src/audiorecorder.cpp
        src/autokjserverapi.cpp
        src/songcanonicalizer.cpp
        src/singeridentitymanager.cpp
        src/rotationfairnessengine.cpp
        src/autobackupmanager.cpp
        src/dlgcheckins.cpp
        src/dlgaliasmanager.cpp
        src/dlgmanagedjs.cpp
        src/dlgmanagevenuesgigs.cpp
        src/widgets/qrcodewidget.cpp
        src/dlgqrcode.cpp
        src/dlgdbupdate.cpp
        src/dlgbookcreator.cpp
        src/dlgeq.cpp
        src/audiofader.cpp
        src/customlineedit.cpp
        src/tickernew.cpp
        src/updatechecker.cpp
        src/videodisplay.cpp
        src/volslider.cpp
        src/dlgaddsinger.cpp
        src/simplecrypt.cpp
        src/dlgsetpassword.cpp
        src/dlgpassword.cpp
        src/dlgpurchaseprogress.cpp
        src/karaokefileinfo.cpp
        src/karaokefilepatternresolver.cpp
        src/custompattern.cpp
        src/dlgeditsong.cpp
        src/soundfxbutton.cpp
        src/runguard/runguard.cpp
        src/durationlazyupdater.cpp
        src/idledetect.cpp
        src/testing/autokjserverclientmock.cpp
        src/mainwindow.h
        src/dlgaddsong.h
        src/dlgvideopreview.h
        src/mediabackend.h
        src/mzarchive.h
        src/okjutil.h
        src/okjtypes.h
        src/dbupdater.h
        src/directorymonitor.h
        src/dlgkeychange.h
        src/dlgdatabase.h
        src/dlgaddvenue.h
        src/dlgrequests.h
        src/dlgregularexport.h
        src/dlgregularimport.h
        src/dlgregularsingers.h
        src/dlgregister.h
        src/dlgsettings.h
        src/dlgcdg.h
        src/okarchive.h
        src/models/tablemodelbreaksongs.h
        src/models/tablemodelcustomnamingpatterns.h
        src/models/tablemodelhistorysingers.h
        src/models/tablemodelhistorysongs.h
        src/models/tablemodelkaraokesongs.h
        src/models/tablemodelkaraokesourcedirs.h
        src/models/tablemodelplaylistsongs.h
        src/models/tablemodelqueuesongs.h
        src/models/tablemodelrequests.h
        src/models/tablemodelrotation.h

        src/tagreader.h
        src/bmdbupdatethread.h
        src/settings.h
        src/bmdbdialog.h
        src/dlgcustompatterns.h
        src/audiorecorder.h
        src/autokjserverapi.h
        src/autokjserverclient.h
        src/dlgdbupdate.h
        src/dlgmanagedjs.h
        src/dlgmanagevenuesgigs.h
        src/dlgbookcreator.h
        src/dlgeq.h
        src/audiofader.h
        src/customlineedit.h
        src/tickernew.h
        src/updatechecker.h
        src/videodisplay.h
        src/volslider.h
        src/okjversion.h
        src/dlgaddsinger.h
        src/simplecrypt.h
        src/dlgsetpassword.h
        src/dlgpassword.h
        src/dlgpurchaseprogress.h
        src/karaokefileinfo.h
        src/karaokefilepatternresolver.h
        src/custompattern.h
        src/dlgeditsong.h
        src/soundfxbutton.h
        src/runguard/runguard.h
        src/models/tableviewtooltipfilter.h
        src/durationlazyupdater.h
        src/idledetect.h
        src/testing/autokjserverclientmock.h
        src/mainwindow.ui
        src/dlgaddsong.ui
        src/dlgkeychange.ui
        src/dlgdatabase.ui
        src/dlgaddvenue.ui
        src/dlgrequests.ui
        src/dlgregularexport.ui
        src/dlgregularimport.ui
        src/dlgregularsingers.ui
        src/dlgregister.ui
        src/dlgsettings.ui
        src/dlgcdg.ui
        src/bmdbdialog.ui
        src/dlgcustompatterns.ui
        src/dlgdbupdate.ui
        src/dlgbookcreator.ui
        src/dlgeq.ui
        src/dlgaddsinger.ui
        src/dlgsetpassword.ui
        src/dlgpassword.ui
        src/dlgpurchaseprogress.ui
        src/dlgeditsong.ui
        src/dlgvideopreview.ui
        src/resources.qrc
        src/softwarerendervideosink.cpp
        src/softwarerendervideosink.h
        src/cdg/cdgappsrc.cpp
        src/cdg/cdgappsrc.h
        src/cdg/cdgfilereader.cpp
        src/cdg/cdgfilereader.h
        src/cdg/cdgimageframe.cpp
        src/cdg/cdgimageframe.h
        src/cdg/libCDG.h
        src/gstreamer/gstreamerhelper.cpp
        src/gstreamer/gstreamerhelper.h

        # ── Streaming service wrappers ───────────────────────────────────────
        src/streaming/streamingcontroller.h
        src/streaming/spotifycontroller.h
        src/streaming/spotifycontroller.cpp
        src/streaming/youtubemusiccontroller.h
        src/streaming/youtubemusiccontroller.cpp
        src/streaming/itunescontroller.h
        src/streaming/itunescontroller.cpp
        src/dlgstreamingservices.h
        src/dlgstreamingservices.cpp
        src/dlgstreamingservices.ui
        )

set(LIBRARIES
        spdlog
        Qt5::Widgets
        Qt5::Core
        Qt5::Gui
        Qt5::Sql
        Qt5::Network
        Qt5::WebSockets
        Qt5::Svg
        Qt5::PrintSupport
        Qt5::Concurrent
        Qt5::WebChannel
        Qt5::WebEngineWidgets
        )

# ── Platform-specific streaming deps ─────────────────────────────────────────
if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    find_package(Qt5 COMPONENTS AxContainer QUIET)
    if (Qt5AxContainer_FOUND)
        list(APPEND LIBRARIES Qt5::AxContainer)
        add_compile_definitions(HAVE_QT_AXCONTAINER)
        message(STATUS "Qt5::AxContainer found — iTunes COM support enabled")
    else ()
        message(STATUS "Qt5::AxContainer not found — iTunes COM support disabled")
    endif ()
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    find_package(Qt5 COMPONENTS DBus QUIET)
    if (Qt5DBus_FOUND)
        list(APPEND LIBRARIES Qt5::DBus)
        add_compile_definitions(HAVE_QT_DBUS)
        message(STATUS "Qt5::DBus found — Linux MPRIS support enabled")
    endif ()
endif ()


if (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")

    if (EXTERNAL_TAGLIB)
        list(APPEND LIBRARIES "PkgConfig::TAGLIB")
    endif ()

    if (EXTERNAL_SPDLOG)
        list(APPEND LIBRARIES "PkgConfig::SPDLOG")
    endif ()

    add_executable(auto-kj
            ${SOURCE_FILES}
            )
    target_link_libraries(auto-kj ${LIBRARIES} ${GSTREAMER_LIBRARIES} qrencode)

    install(
            TARGETS auto-kj
            DESTINATION bin
    )
    install(
            FILES src/Icons/org.autokj.AutoKJ.svg
            DESTINATION share/icons/hicolor/scalable/apps
    )
    install(
            FILES src/org.autokj.AutoKJ.desktop
            DESTINATION share/applications
    )
    install(
            FILES src/org.autokj.AutoKJ.metainfo.xml
            DESTINATION share/metainfo
    )
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(autokj_ICON ${CMAKE_CURRENT_SOURCE_DIR}/src/Icons/Auto-KJ.icns)

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-app-1.0 gstreamer-audio-1.0 gstreamer-pbutils-1.0 gstreamer-controller-1.0 gstreamer-video-1.0)

    set_source_files_properties(${autokj_ICON} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources"
            )
    add_executable(auto-kj
            MACOSX_BUNDLE
            ${autokj_ICON}
            ${SOURCE_FILES}
            )
    set_target_properties(auto-kj PROPERTIES
            BUNDLE True
            MACOSX_BUNDLE_GUI_IDENTIFIER org.autokj.autokj
            MACOSX_BUNDLE_BUNDLE_NAME Auto-KJ
            MACOSX_BUNDLE_BUNDLE_VERSION ${sem_ver}
            MACOSX_BUNDLE_ICON_FILE Auto-KJ.icns
            )
    target_link_libraries(auto-kj
            ${LIBRARIES}
            qrencode
	    #"-F /Library/Frameworks -framework GStreamer"
	    ${GSTREAMER_LIBRARIES}
            ${GSTREAMER_FRAMEWORK}
            )
    if (NOT DEFINED BUILDONLY)
        get_target_property(_qt5_qmake_location Qt5::qmake IMPORTED_LOCATION)
        execute_process(
                COMMAND "${_qt5_qmake_location}" -query QT_INSTALL_PREFIX
                RESULT_VARIABLE return_code
                OUTPUT_VARIABLE qt5_install_prefix
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if (qt5_install_prefix)
            find_program(MACDEPLOYQT macdeployqt HINTS ${qt5_install_prefix} PATH_SUFFIXES bin)
            if (MACDEPLOYQT)
                if (DEFINED MAC_SIGNING_IDENTITY)
                    add_custom_command(
                            TARGET auto-kj
                            COMMAND "${MACDEPLOYQT}"
                            VERBATIM
                            ARGS
                            "${CMAKE_BINARY_DIR}/Auto-KJ.app"
                            -codesign=${MAC_SIGNING_IDENTITY}
                            "-appstore-compliant"
                    )
                else ()
                    add_custom_command(
                            TARGET auto-kj
                            COMMAND "${MACDEPLOYQT}"
                            ARGS "${CMAKE_BINARY_DIR}/Auto-KJ.app"
                    )
                endif ()
            else ()
                message("Can't find macdeployqt")
            endif ()
        else ()
            message("QT_INSTALL_DIR not defined")
        endif ()
        if (DEFINED BUNDLE_GSTREAMER)
            message("Installing GStreamer framework into app bundle")
            install(
                    DIRECTORY "/Library/Frameworks/GStreamer.framework.dist"
                    DESTINATION "Auto-KJ.app/Contents/Frameworks/GStreamer.framework"
            )
            add_custom_command(
                    TARGET auto-kj
                    POST_BUILD
                    COMMAND "cp"
                    ARGS
                    "-pR"
                    "/Library/Frameworks/GStreamer.framework.dist"
                    "Auto-KJ.app/Contents/Frameworks/GStreamer.framework"
                    COMMAND "osxrelocator"
                    ARGS
                    "Auto-KJ.app/Contents/Frameworks/GStreamer.framework/Versions/Current/lib"
                    "/Library/Frameworks/GStreamer.framework/"
                    "@executable_path/../Frameworks/GStreamer.framework/"
                    "-r"
                    USES_TERMINAL
                    COMMAND "osxrelocator"
                    ARGS
                    "Auto-KJ.app/Contents/Frameworks/GStreamer.framework/Versions/Current/libexec"
                    "/Library/Frameworks/GStreamer.framework/"
                    "@executable_path/../../../../../../Frameworks/GStreamer.framework/"
                    "-r"
                    USES_TERMINAL
                    COMMAND "osxrelocator"
                    ARGS
                    "Auto-KJ.app/Contents/Frameworks/GStreamer.framework/Versions/Current/bin"
                    "/Library/Frameworks/GStreamer.framework/"
                    "@executable_path/../../../../Frameworks/GStreamer.framework/"
                    "-r"
                    USES_TERMINAL
                    COMMAND "osxrelocator"
                    ARGS
                    "Auto-KJ.app/Contents/MacOS"
                    "/Library/Frameworks/GStreamer.framework/"
                    "@executable_path/../Frameworks/GStreamer.framework/"
                    "-r"
                    USES_TERMINAL
            )
        endif ()
    endif ()
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    add_executable(auto-kj
            ${SOURCE_FILES}
            ${WINDOWS_ICON_RC}
            )
    target_link_libraries(auto-kj PRIVATE ${LIBRARIES} qrencode)
    set_property(
            TARGET auto-kj
            PROPERTY WIN32_EXECUTABLE true
    )

    if (DEFINED DEPLOY_DEPS)
        get_target_property(_qt5_qmake_location Qt5::qmake IMPORTED_LOCATION)
        execute_process(
                COMMAND "${_qt5_qmake_location}" -query QT_INSTALL_PREFIX
                RESULT_VARIABLE return_code
                OUTPUT_VARIABLE qt5_install_prefix
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        install(DIRECTORY "${GST_BASE_PATH}/bin/" DESTINATION "bin")
        if (qt5_install_prefix)
            find_program(WINDEPLOYQT windeployqt HINTS ${qt5_install_prefix} PATH_SUFFIXES bin)
            if (WINDEPLOYQT)
                add_custom_command(
                        TARGET auto-kj POST_BUILD
                        COMMAND "${WINDEPLOYQT}"
                        ARGS "$<TARGET_FILE:auto-kj>"
                )
            else ()
                message("Can't find windeployqt")
            endif ()

            # Copy OpenSSL 1.1 runtime DLLs required by Qt 5.15 for HTTPS/TLS.
            # Look in Qt bin dir first, then the default OpenSSL Win64 install location.
            foreach(_ssl_dll "libssl-1_1-x64.dll" "libcrypto-1_1-x64.dll")
                set(_ssl_found FALSE)
                foreach(_ssl_dir "${qt5_install_prefix}/bin" "C:/Program Files/OpenSSL-Win64/bin" "C:/Program Files/OpenSSL/bin")
                    if(EXISTS "${_ssl_dir}/${_ssl_dll}")
                        add_custom_command(
                            TARGET auto-kj POST_BUILD
                            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                "${_ssl_dir}/${_ssl_dll}"
                                "$<TARGET_FILE_DIR:auto-kj>/${_ssl_dll}"
                        )
                        message(STATUS "OpenSSL: will copy ${_ssl_dll} from ${_ssl_dir}")
                        set(_ssl_found TRUE)
                        break()
                    endif()
                endforeach()
                if(NOT _ssl_found)
                    message(WARNING
                        "OpenSSL 1.1 DLL not found: ${_ssl_dll}. "
                        "TLS/HTTPS will fail at runtime. "
                        "Install Win64 OpenSSL 1.1 from https://slproweb.com/products/Win32OpenSSL.html "
                        "or run: winget install ShiningLight.OpenSSL.Light")
                endif()
            endforeach()
        else ()
            message("QT_INSTALL_DIR not defined")
        endif ()
        add_custom_command(
                TARGET auto-kj POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_directory "${GST_BASE_PATH}/bin" "$<TARGET_FILE_DIR:auto-kj>"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:auto-kj>/lib/gstreamer-1.0"
                COMMAND "${CMAKE_COMMAND}" -E copy_directory "${GST_BASE_PATH}/lib/gstreamer-1.0" "$<TARGET_FILE_DIR:auto-kj>/lib/gstreamer-1.0"
        )
    endif ()
endif ()


