/*
 * GOD'S EYE VIEW — settings for this copy of the site.
 *
 * planesRelay is the web address of the planes relay (godview/relay/): every
 * free flight-data service refuses requests from web pages, so PLANES only
 * works through a relay. Empty means none is set up yet, and PLANES says
 * OFFLINE.
 */
(function (global) {
  'use strict';

  global.GV_CONFIG = {
    planesRelay: 'https://godview-planes.cody-collins85-cc.workers.dev'
  };
})(typeof globalThis !== 'undefined' ? globalThis : this);
