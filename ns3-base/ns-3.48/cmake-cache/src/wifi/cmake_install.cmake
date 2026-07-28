# Install script for directory: /home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.48-wifi-optimized.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.48-wifi-optimized.so")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.48-wifi-optimized.so"
         RPATH "/usr/local/lib:\$ORIGIN/:\$ORIGIN/../lib:/usr/local/lib64:\$ORIGIN/:\$ORIGIN/../lib64")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/build/lib/libns3.48-wifi-optimized.so")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.48-wifi-optimized.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.48-wifi-optimized.so")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.48-wifi-optimized.so"
         OLD_RPATH "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/build/lib:"
         NEW_RPATH "/usr/local/lib:\$ORIGIN/:\$ORIGIN/../lib:/usr/local/lib64:\$ORIGIN/:\$ORIGIN/../lib64")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.48-wifi-optimized.so")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ns3" TYPE FILE FILES
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/helper/athstats-helper.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/helper/spectrum-wifi-helper.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/helper/wifi-co-trace-helper.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/helper/wifi-helper.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/helper/wifi-mac-helper.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/helper/wifi-radio-energy-model-helper.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/helper/yans-wifi-helper.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/helper/wifi-phy-rx-trace-helper.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/helper/wifi-static-setup-helper.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/helper/wifi-tx-stats-helper.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/addba-extension.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/adhoc-wifi-mac.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/ampdu-subframe-header.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/ampdu-tag.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/amsdu-subframe-header.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/ap-wifi-mac.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/block-ack-agreement.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/block-ack-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/block-ack-type.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/block-ack-window.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/capability-information.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/channel-access-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/ctrl-headers.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/default-power-save-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/edca-parameter-set.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/eht/advanced-ap-emlsr-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/eht/advanced-emlsr-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/eht/ap-emlsr-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/eht/common-info-basic-mle.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/eht/common-info-probe-req-mle.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/eht/default-ap-emlsr-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/eht/default-emlsr-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/eht/eht-capabilities.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/eht/eht-configuration.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/eht/eht-frame-exchange-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/eht/eht-operation.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/eht/tid-to-link-mapping-element.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/eht/eht-phy.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/eht/eht-ppdu.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/eht/eht-ru.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/eht/emlsr-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/eht/multi-link-element.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/error-rate-model.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/extended-capabilities.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/fcfs-wifi-queue-scheduler.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/frame-capture-model.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/frame-exchange-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/gcr-group-address.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/gcr-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/he/constant-obss-pd-algorithm.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/he/he-6ghz-band-capabilities.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/he/he-capabilities.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/he/he-configuration.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/he/he-frame-exchange-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/he/he-operation.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/he/he-phy.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/he/he-ppdu.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/he/he-ru.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/he/mu-edca-parameter-set.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/he/mu-snr-tag.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/he/multi-user-scheduler.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/he/obss-pd-algorithm.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/he/rr-multi-user-scheduler.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/ht/ht-capabilities.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/ht/ht-configuration.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/ht/ht-frame-exchange-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/ht/ht-operation.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/ht/ht-phy.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/ht/ht-ppdu.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/interference-helper.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/mac-rx-middle.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/mac-tx-middle.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/mgt-action-headers.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/mgt-headers.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/mpdu-aggregator.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/msdu-aggregator.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/nist-error-rate-model.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/non-ht/dsss-error-rate-model.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/non-ht/dsss-parameter-set.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/non-ht/dsss-phy.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/non-ht/dsss-ppdu.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/non-ht/erp-information.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/non-ht/erp-ofdm-phy.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/non-ht/erp-ofdm-ppdu.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/non-ht/ofdm-phy.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/non-ht/ofdm-ppdu.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/non-inheritance.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/originator-block-ack-agreement.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/phy-entity.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/power-save-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/preamble-detection-model.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/qos-frame-exchange-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/qos-txop.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/qos-utils.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/rate-control/aarf-wifi-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/rate-control/aarfcd-wifi-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/rate-control/amrr-wifi-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/rate-control/aparf-wifi-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/rate-control/arf-wifi-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/rate-control/cara-wifi-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/rate-control/constant-rate-wifi-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/rate-control/ideal-wifi-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/rate-control/minstrel-ht-wifi-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/rate-control/minstrel-wifi-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/rate-control/onoe-wifi-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/rate-control/parf-wifi-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/rate-control/rraa-wifi-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/rate-control/rrpaa-wifi-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/rate-control/thompson-sampling-wifi-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/recipient-block-ack-agreement.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/reduced-neighbor-report.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/reference/error-rate-tables.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/rr-wifi-queue-scheduler.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/simple-frame-capture-model.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/snr-tag.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/spectrum-wifi-phy.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/ssid.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/sta-wifi-mac.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/status-code.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/supported-rates.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/table-based-error-rate-model.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/threshold-preamble-detection-model.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/tim.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/txop.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/vht/vht-capabilities.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/vht/vht-configuration.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/vht/vht-frame-exchange-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/vht/vht-operation.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/vht/vht-phy.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/vht/vht-ppdu.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-ack-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-acknowledgment.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-assoc-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-bandwidth-filter.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-default-ack-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-default-assoc-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-default-gcr-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-default-protection-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-information-element.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-mac-header.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-mac-queue-container.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-mac-queue-elem.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-mac-queue-scheduler-impl.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-mac-queue-scheduler.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-mac-queue.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-mac-trailer.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-mac.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-mgt-header.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-mode.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-mpdu.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-net-device.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-ns3-constants.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-opt-field.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-phy-band.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-phy-common.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-phy-listener.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-phy-operating-channel.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-phy-state-helper.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-phy-state.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-phy.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-ppdu.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-protection-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-protection.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-psdu.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-radio-energy-model.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-remote-station-info.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-remote-station-manager.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-ru.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-spectrum-phy-interface.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-spectrum-signal-parameters.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-standard-constants.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-standards.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-tx-current-model.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-tx-parameters.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-tx-timer.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-tx-vector.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-types.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-units.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-utils.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/yans-error-rate-model.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/yans-wifi-channel.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/yans-wifi-phy.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/src/wifi/model/wifi-spectrum-value-helper.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/build/include/ns3/wifi-module.h"
    "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/build/include/ns3/wifi-export.h"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/akay/Documents/1_Projects/capstone/cap_proving_gnd/ns3-base/ns-3.48/cmake-cache/src/wifi/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
