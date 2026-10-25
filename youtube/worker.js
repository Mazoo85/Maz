/*
 * YouTube for Claude - a small MCP server that lets Claude look things up on
 * YouTube from any session (web, phone, desktop, cloud).
 *
 * It runs as a free Cloudflare Worker and asks Google's official YouTube Data
 * API v3 with an API key. Claude reaches it as a "custom connector" in
 * claude.ai, speaking the Model Context Protocol (MCP) over plain HTTPS:
 *
 *   POST /mcp/<password>   MCP (JSON-RPC 2.0) - initialize, tools/list, tools/call
 *   GET  /check            is the key set, does YouTube answer? (no secrets shown)
 *
 * It is read-only and pointed at REUSABLE videos: searches default to videos
 * their owners marked Creative Commons (CC BY), which may be reused with
 * credit. Every video in every answer says whether it is reusable and, when it
 * is, carries a ready-made credit line. It never downloads a video.
 *
 * Secrets (Cloudflare: Settings -> Variables and Secrets):
 *   YOUTUBE_API_KEY      from Google Cloud (YouTube Data API v3 enabled)
 *   CONNECTOR_PASSWORD   anything long and private; goes in the connector URL
 *
 * Self-contained on purpose: the whole file is pasted into the Cloudflare
 * dashboard as it is. Setup steps are in README.md beside it. The pure helpers
 * are exported so tests/worker.test.mjs can check them without a network.
 */

const SERVER_NAME = 'youtube-for-claude';
const SERVER_VERSION = '1.0.0';
const PROTOCOL_VERSIONS = ['2025-06-18', '2025-03-26', '2024-11-05'];
const API = 'https://www.googleapis.com/youtube/v3/';
const UPSTREAM_TIMEOUT_MS = 10000;
const DAILY_QUOTA = 10000;

const INSTRUCTIONS =
  'Read-only YouTube lookups through the official YouTube Data API. Searches default to ' +
  'Creative Commons (reusable) videos; pass license "any" to widen. Each video says whether it ' +
  'is reusable and carries a credit line to use when it is. Google allows ' + DAILY_QUOTA +
  ' quota units a day (search costs about 100, everything else about 1), so prefer get_video, ' +
  'list_channel_videos and trending_videos over repeated searches. For videos that are reusable AND ' +
  'trending (rising fast right now), use rising_reusable_videos: the trending chart itself almost never ' +
  'has a reusable video. This server never downloads ' +
  'videos or uploads anything.';

/* ------------------------------------------------------------------ links */

const VIDEO_ID = /^[A-Za-z0-9_-]{11}$/;
const CHANNEL_ID = /^UC[A-Za-z0-9_-]{22}$/;

function parseUrl(text) {
  const s = String(text || '').trim();
  if (!/^https?:\/\//i.test(s)) return null;
  try {
    return new URL(s);
  } catch (e) {
    return null;
  }
}

function hostIsYouTube(url) {
  const h = url.hostname.toLowerCase().replace(/^(www|m|music)\./, '');
  return h === 'youtube.com' || h === 'youtu.be' || h === 'youtube-nocookie.com';
}

/* A video id from a bare id or any YouTube video link; null if it is neither. */
export function parseVideoId(text) {
  const s = String(text || '').trim();
  if (VIDEO_ID.test(s)) return s;
  const url = parseUrl(s);
  if (!url || !hostIsYouTube(url)) return null;
  const parts = url.pathname.split('/').filter(Boolean);
  let id = url.searchParams.get('v');
  if (!id && url.hostname.toLowerCase() === 'youtu.be') id = parts[0];
  if (!id && parts.length >= 2 && ['shorts', 'live', 'embed', 'v'].includes(parts[0])) id = parts[1];
  return id && VIDEO_ID.test(id) ? id : null;
}

/* How to ask the API for a channel: { id } for channel ids and /channel/ links,
 * { forHandle } for @handles and /@handle links, { forUsername } for /user/
 * links. Null if the text is none of those. */
export function parseChannel(text) {
  const s = String(text || '').trim();
  if (CHANNEL_ID.test(s)) return { id: s };
  if (/^@[\w.-]{3,}$/.test(s)) return { forHandle: s };
  const url = parseUrl(s);
  if (!url || !hostIsYouTube(url)) return /^[\w.-]{3,}$/.test(s) ? { forHandle: '@' + s } : null;
  const parts = url.pathname.split('/').filter(Boolean);
  if (parts[0] === 'channel' && CHANNEL_ID.test(parts[1] || '')) return { id: parts[1] };
  if (parts[0] && parts[0].startsWith('@') && /^@[\w.-]{3,}$/.test(parts[0])) return { forHandle: parts[0] };
  if (parts[0] === 'user' && parts[1]) return { forUsername: parts[1] };
  if (parts[0] === 'c' && parts[1]) return { forHandle: '@' + parts[1] };
  return null;
}

/* A playlist id from a bare id or a link with ?list=; null otherwise. */
export function parsePlaylistId(text) {
  const s = String(text || '').trim();
  const url = parseUrl(s);
  const id = url ? (hostIsYouTube(url) ? url.searchParams.get('list') : null) : s;
  return id && /^[A-Za-z0-9_-]{10,}$/.test(id) ? id : null;
}

/* ISO 8601 "PT1H2M3S" -> seconds; null if unreadable. */
export function parseDuration(iso) {
  const m = /^P(?:(\d+)D)?(?:T(?:(\d+)H)?(?:(\d+)M)?(?:(\d+)S)?)?$/.exec(String(iso || ''));
  if (!m) return null;
  const [, d, h, mi, s] = m.map((x) => Number(x || 0));
  return d * 86400 + h * 3600 + mi * 60 + s;
}

export function formatDuration(seconds) {
  if (seconds === null || !Number.isFinite(seconds)) return null;
  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const s = seconds % 60;
  const mm = h ? String(m).padStart(2, '0') : String(m);
  return (h ? h + ':' : '') + mm + ':' + String(s).padStart(2, '0');
}

/* ---------------------------------------------------------- result shapes */

function count(value) {
  if (value === undefined || value === null || value === '') return null;
  const n = Number(value);
  return Number.isFinite(n) ? n : null;
}

function trim(text, max) {
  const s = String(text || '');
  return s.length > max ? s.slice(0, max - 1).trimEnd() + '…' : s;
}

/* Views divided by days online (a video under a day old counts as one day),
 * so a new video rising fast ranks above an old one that took years. */
export function viewsPerDay(views, published, now) {
  const at = Date.parse(published || '');
  if (views === null || !Number.isFinite(at)) return null;
  const days = Math.max(1, ((now === undefined ? Date.now() : now) - at) / 86400000);
  return Math.round(views / days);
}

/* One video, the same shape in every answer, from a videos.list item. */
export function shapeVideo(item) {
  const sn = item.snippet || {};
  const st = item.statistics || {};
  const license = (item.status && item.status.license) || 'youtube';
  const reusable = license === 'creativeCommon';
  const url = 'https://www.youtube.com/watch?v=' + item.id;
  const channelUrl = sn.channelId ? 'https://www.youtube.com/channel/' + sn.channelId : null;
  const seconds = parseDuration(item.contentDetails && item.contentDetails.duration);
  const video = {
    id: item.id,
    url: url,
    title: sn.title || '',
    channel: { id: sn.channelId || null, title: sn.channelTitle || '', url: channelUrl },
    published: sn.publishedAt || null,
    duration: { seconds: seconds, text: formatDuration(seconds) },
    views: count(st.viewCount),
    likes: count(st.likeCount),
    comments: count(st.commentCount),
    views_per_day: viewsPerDay(count(st.viewCount), sn.publishedAt),
    license: license,
    reusable: reusable,
    description: trim(sn.description, 500)
  };
  if (reusable) video.credit = 'Video by ' + video.channel.title + ' (' + url + '), licensed CC BY';
  if (Array.isArray(sn.tags) && sn.tags.length) video.tags = sn.tags.slice(0, 20);
  return video;
}

export function shapeChannel(item) {
  const sn = item.snippet || {};
  const st = item.statistics || {};
  const related = (item.contentDetails && item.contentDetails.relatedPlaylists) || {};
  return {
    id: item.id,
    url: 'https://www.youtube.com/channel/' + item.id,
    title: sn.title || '',
    handle: sn.customUrl || null,
    country: sn.country || null,
    created: sn.publishedAt || null,
    subscribers: st.hiddenSubscriberCount ? null : count(st.subscriberCount),
    videos: count(st.videoCount),
    views: count(st.viewCount),
    uploads_playlist: related.uploads || null,
    description: trim(sn.description, 500)
  };
}

export function shapeComment(thread) {
  const top = (thread.snippet && thread.snippet.topLevelComment && thread.snippet.topLevelComment.snippet) || {};
  return {
    id: thread.id,
    author: top.authorDisplayName || '',
    text: trim(top.textOriginal || top.textDisplay, 1000),
    likes: count(top.likeCount),
    published: top.publishedAt || null,
    replies: count(thread.snippet && thread.snippet.totalReplyCount)
  };
}

/* ------------------------------------------------------------- the API */

export class YouTubeError extends Error {}

/* Google's error reasons turned into one plain sentence Claude can repeat. */
export function explainApiError(status, body) {
  const err = (body && body.error) || {};
  const reasons = (err.errors || []).map((e) => e.reason);
  const message = err.message || '';
  if (reasons.includes('quotaExceeded') || reasons.includes('dailyLimitExceeded') || reasons.includes('rateLimitExceeded')) {
    return "YouTube's free daily allowance is used up for today. It resets at midnight Pacific time.";
  }
  if (reasons.includes('keyInvalid') || /API key not valid/i.test(message)) {
    return 'The YouTube API key is wrong. Check the YOUTUBE_API_KEY secret on the Cloudflare worker.';
  }
  if (reasons.includes('accessNotConfigured') || /has not been used|is disabled/i.test(message)) {
    return 'YouTube Data API v3 is not enabled for this API key. Enable it in the Google Cloud console.';
  }
  if (reasons.includes('ipRefererBlocked') || reasons.includes('forbidden') && /referer|restrict/i.test(message)) {
    return 'The API key is restricted to other websites or apps. Restrict it only by API (YouTube Data API v3).';
  }
  if (reasons.includes('commentsDisabled')) return 'Comments are turned off on that video.';
  if (reasons.includes('playlistNotFound')) return 'There is no public playlist with that id.';
  if (reasons.includes('videoNotFound')) return 'There is no video with that id.';
  if (reasons.includes('channelNotFound')) return 'There is no channel with that id or handle.';
  return 'YouTube answered ' + status + (message ? ': ' + message : '') + '.';
}

async function youtube(env, path, params) {
  const key = secret(env, 'YOUTUBE_API_KEY');
  if (!key) {
    throw new YouTubeError('No YouTube API key is set. Add the YOUTUBE_API_KEY secret on the Cloudflare worker.');
  }
  const url = new URL(API + path);
  for (const [k, v] of Object.entries(params)) {
    if (v !== undefined && v !== null && v !== '') url.searchParams.set(k, String(v));
  }
  url.searchParams.set('key', key);
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), UPSTREAM_TIMEOUT_MS);
  let res;
  try {
    res = await fetch(url.toString(), { headers: { Accept: 'application/json' }, signal: controller.signal });
  } catch (e) {
    throw new YouTubeError(e && e.name === 'AbortError' ? 'YouTube took too long to answer.' : 'Could not reach YouTube: ' + (e && e.message || e));
  } finally {
    clearTimeout(timer);
  }
  let body = null;
  try {
    body = await res.json();
  } catch (e) {
    body = null;
  }
  if (!res.ok) throw new YouTubeError(explainApiError(res.status, body));
  return body || {};
}

/* Full details for up to 50 video ids, in the order asked. */
async function videoDetails(env, ids) {
  if (!ids.length) return [];
  const body = await youtube(env, 'videos', {
    part: 'snippet,contentDetails,statistics,status',
    id: ids.slice(0, 50).join(','),
    maxResults: 50
  });
  const byId = new Map((body.items || []).map((item) => [item.id, item]));
  return ids.map((id) => byId.get(id)).filter(Boolean).map(shapeVideo);
}

async function findChannel(env, text) {
  const how = parseChannel(text);
  if (!how) throw new YouTubeError('That is not a channel id, @handle or channel link.');
  const body = await youtube(env, 'channels', Object.assign({ part: 'snippet,statistics,contentDetails', maxResults: 1 }, how));
  const item = (body.items || [])[0];
  if (!item) throw new YouTubeError('There is no channel with that id or handle.');
  return shapeChannel(item);
}

async function playlistVideoIds(env, playlistId, max) {
  const body = await youtube(env, 'playlistItems', { part: 'contentDetails', playlistId: playlistId, maxResults: max });
  return (body.items || []).map((i) => i.contentDetails && i.contentDetails.videoId).filter(Boolean);
}

/* --------------------------------------------------------------- tools */

function clampInt(value, fallback, lo, hi) {
  const n = Math.trunc(Number(value));
  if (!Number.isFinite(n)) return fallback;
  return Math.max(lo, Math.min(hi, n));
}

function requireText(args, key, what) {
  const v = args[key];
  if (typeof v !== 'string' || !v.trim()) throw new YouTubeError('Give ' + what + ' as "' + key + '".');
  return v.trim();
}

function licenseOf(args) {
  const l = args.license === undefined ? 'creative_commons' : String(args.license);
  if (l !== 'creative_commons' && l !== 'any') throw new YouTubeError('license must be "creative_commons" or "any".');
  return l;
}

function onlyReusable(videos, license) {
  return license === 'creative_commons' ? videos.filter((v) => v.reusable) : videos;
}

const LICENSE_SCHEMA = {
  type: 'string',
  enum: ['creative_commons', 'any'],
  description: 'creative_commons (default): only videos marked Creative Commons, which may be reused with credit. any: every video.'
};
const SEARCH_ORDERS = { relevance: 'relevance', date: 'date', views: 'viewCount', rating: 'rating' };
const DURATIONS = { any: 'any', short: 'short', medium: 'medium', long: 'long' };

export const TOOLS = [
  {
    name: 'search_videos',
    description: 'Search YouTube for videos. Defaults to Creative Commons (reusable) videos; license "any" widens it. Costs about 100 of the 10,000 daily quota units, so use it for discovery and the cheaper tools for details.',
    inputSchema: {
      type: 'object',
      properties: {
        query: { type: 'string', description: 'What to search for.' },
        license: LICENSE_SCHEMA,
        order: { type: 'string', enum: Object.keys(SEARCH_ORDERS), description: 'relevance (default), date, views or rating.' },
        duration: { type: 'string', enum: Object.keys(DURATIONS), description: 'short (under 4 min), medium (4-20 min), long (over 20 min), any (default).' },
        published_after: { type: 'string', description: 'Only videos published after this date, e.g. 2026-01-01 or 2026-01-01T00:00:00Z.' },
        channel_id: { type: 'string', description: 'Only videos from this channel id (UC...).' },
        region_code: { type: 'string', description: 'Two-letter country code, e.g. US, GB.' },
        max_results: { type: 'integer', minimum: 1, maximum: 25, description: 'How many to return, 1-25 (default 10).' }
      },
      required: ['query']
    },
    async run(args, env) {
      const query = requireText(args, 'query', 'what to search for');
      const license = licenseOf(args);
      const order = SEARCH_ORDERS[args.order || 'relevance'];
      const duration = DURATIONS[args.duration || 'any'];
      if (!order) throw new YouTubeError('order must be one of ' + Object.keys(SEARCH_ORDERS).join(', ') + '.');
      if (!duration) throw new YouTubeError('duration must be one of ' + Object.keys(DURATIONS).join(', ') + '.');
      let after = args.published_after ? String(args.published_after) : null;
      if (after && /^\d{4}-\d{2}-\d{2}$/.test(after)) after += 'T00:00:00Z';
      if (after && Number.isNaN(Date.parse(after))) throw new YouTubeError('published_after must be a date like 2026-01-01.');
      const max = clampInt(args.max_results, 10, 1, 25);
      const body = await youtube(env, 'search', {
        part: 'snippet',
        type: 'video',
        q: query,
        maxResults: max,
        order: order,
        videoLicense: license === 'creative_commons' ? 'creativeCommon' : undefined,
        videoDuration: duration === 'any' ? undefined : duration,
        publishedAfter: after ? new Date(after).toISOString() : undefined,
        channelId: args.channel_id ? String(args.channel_id) : undefined,
        regionCode: args.region_code ? String(args.region_code).toUpperCase() : undefined,
        safeSearch: 'moderate'
      });
      const ids = (body.items || []).map((i) => i.id && i.id.videoId).filter(Boolean);
      const videos = onlyReusable(await videoDetails(env, ids), license);
      return { result: { query: query, license: license, total_estimate: count(body.pageInfo && body.pageInfo.totalResults), videos: videos }, quota: 100 + (ids.length ? 1 : 0) };
    }
  },
  {
    name: 'trending_videos',
    description: "YouTube's most popular videos right now in a country. The chart cannot be filtered by license server-side, so by default every video is returned with its reusable flag; reusable_only drops the rest. Costs 1 quota unit.",
    inputSchema: {
      type: 'object',
      properties: {
        region_code: { type: 'string', description: 'Two-letter country code (default US).' },
        category_id: { type: 'string', description: 'Optional YouTube category id, e.g. 10 = Music, 20 = Gaming, 24 = Entertainment, 28 = Science & Technology.' },
        reusable_only: { type: 'boolean', description: 'Keep only Creative Commons videos (default false).' },
        max_results: { type: 'integer', minimum: 1, maximum: 50, description: '1-50 (default 20).' }
      }
    },
    async run(args, env) {
      const region = String(args.region_code || 'US').toUpperCase();
      if (!/^[A-Z]{2}$/.test(region)) throw new YouTubeError('region_code must be a two-letter country code like US.');
      const max = clampInt(args.max_results, 20, 1, 50);
      const body = await youtube(env, 'videos', {
        part: 'snippet,contentDetails,statistics,status',
        chart: 'mostPopular',
        regionCode: region,
        videoCategoryId: args.category_id ? String(args.category_id) : undefined,
        maxResults: max
      });
      let videos = (body.items || []).map(shapeVideo);
      if (args.reusable_only) videos = videos.filter((v) => v.reusable);
      return { result: { region_code: region, reusable_only: !!args.reusable_only, videos: videos }, quota: 1 };
    }
  },
  {
    name: 'rising_reusable_videos',
    description: 'Reusable (Creative Commons) videos that are trending right now: published in the last few days and ranked by views per day, so the fastest-rising come first. Use this when the videos must be reusable AND trending AND popular; the trending chart itself almost never has a reusable video. Optionally narrowed to a topic. Costs about 101 quota units.',
    inputSchema: {
      type: 'object',
      properties: {
        query: { type: 'string', description: 'Optional topic, e.g. "funny animals". Leave out for every topic.' },
        days: { type: 'integer', minimum: 1, maximum: 30, description: 'How far back to look, 1-30 days (default 7).' },
        region_code: { type: 'string', description: 'Optional two-letter country code, e.g. US.' },
        duration: { type: 'string', enum: Object.keys(DURATIONS), description: 'short (under 4 min), medium (4-20 min), long (over 20 min), any (default).' },
        max_results: { type: 'integer', minimum: 1, maximum: 25, description: 'How many to return, 1-25 (default 10).' }
      }
    },
    async run(args, env) {
      const days = clampInt(args.days, 7, 1, 30);
      const max = clampInt(args.max_results, 10, 1, 25);
      const duration = DURATIONS[args.duration || 'any'];
      if (!duration) throw new YouTubeError('duration must be one of ' + Object.keys(DURATIONS).join(', ') + '.');
      const region = args.region_code ? String(args.region_code).toUpperCase() : undefined;
      if (region && !/^[A-Z]{2}$/.test(region)) throw new YouTubeError('region_code must be a two-letter country code like US.');
      const query = typeof args.query === 'string' && args.query.trim() ? args.query.trim() : undefined;
      const since = new Date(Date.now() - days * 86400000).toISOString();
      const body = await youtube(env, 'search', {
        part: 'snippet',
        type: 'video',
        q: query,
        videoLicense: 'creativeCommon',
        order: 'viewCount',
        publishedAfter: since,
        videoDuration: duration === 'any' ? undefined : duration,
        regionCode: region,
        maxResults: 50,
        safeSearch: 'moderate'
      });
      const ids = (body.items || []).map((i) => i.id && i.id.videoId).filter(Boolean);
      const videos = (await videoDetails(env, ids))
        .filter((v) => v.reusable)
        .sort((a, b) => (b.views_per_day || 0) - (a.views_per_day || 0))
        .slice(0, max);
      return { result: { query: query || null, days: days, published_after: since, videos: videos }, quota: 100 + (ids.length ? 1 : 0) };
    }
  },
  {
    name: 'get_video',
    description: 'Details of one video: title, channel, length, views, likes, comment count, license and whether it is reusable. Costs 1 quota unit.',
    inputSchema: {
      type: 'object',
      properties: { video: { type: 'string', description: 'A video id or any YouTube video link.' } },
      required: ['video']
    },
    async run(args, env) {
      const id = parseVideoId(requireText(args, 'video', 'a video id or link'));
      if (!id) throw new YouTubeError('That is not a YouTube video id or link.');
      const videos = await videoDetails(env, [id]);
      if (!videos.length) throw new YouTubeError('There is no video with that id.');
      return { result: videos[0], quota: 1 };
    }
  },
  {
    name: 'get_channel',
    description: 'Details of a channel: title, subscribers, video and view counts, and its uploads playlist id. Costs 1 quota unit.',
    inputSchema: {
      type: 'object',
      properties: { channel: { type: 'string', description: 'A channel id (UC...), an @handle, or a channel link.' } },
      required: ['channel']
    },
    async run(args, env) {
      const channel = await findChannel(env, requireText(args, 'channel', 'a channel id, @handle or link'));
      return { result: channel, quota: 1 };
    }
  },
  {
    name: 'list_channel_videos',
    description: "A channel's latest uploads, newest first, with full details. Defaults to reusable (Creative Commons) videos only. Costs 3 quota units.",
    inputSchema: {
      type: 'object',
      properties: {
        channel: { type: 'string', description: 'A channel id (UC...), an @handle, or a channel link.' },
        license: LICENSE_SCHEMA,
        max_results: { type: 'integer', minimum: 1, maximum: 50, description: '1-50 (default 20). With the Creative Commons filter, the newest 50 uploads are checked and the reusable ones returned.' }
      },
      required: ['channel']
    },
    async run(args, env) {
      const license = licenseOf(args);
      const max = clampInt(args.max_results, 20, 1, 50);
      const channel = await findChannel(env, requireText(args, 'channel', 'a channel id, @handle or link'));
      if (!channel.uploads_playlist) throw new YouTubeError('That channel has no public uploads.');
      const ids = await playlistVideoIds(env, channel.uploads_playlist, license === 'any' ? max : 50);
      const videos = onlyReusable(await videoDetails(env, ids), license).slice(0, max);
      return { result: { channel: { id: channel.id, title: channel.title, url: channel.url }, license: license, videos: videos }, quota: 2 + (ids.length ? 1 : 0) };
    }
  },
  {
    name: 'list_playlist_videos',
    description: 'The videos in a public playlist, with full details. Defaults to reusable (Creative Commons) videos only. Costs 2 quota units.',
    inputSchema: {
      type: 'object',
      properties: {
        playlist: { type: 'string', description: 'A playlist id or a playlist link.' },
        license: LICENSE_SCHEMA,
        max_results: { type: 'integer', minimum: 1, maximum: 50, description: '1-50 (default 20). With the Creative Commons filter, the first 50 items are checked and the reusable ones returned.' }
      },
      required: ['playlist']
    },
    async run(args, env) {
      const license = licenseOf(args);
      const max = clampInt(args.max_results, 20, 1, 50);
      const playlistId = parsePlaylistId(requireText(args, 'playlist', 'a playlist id or link'));
      if (!playlistId) throw new YouTubeError('That is not a playlist id or link.');
      const ids = await playlistVideoIds(env, playlistId, license === 'any' ? max : 50);
      const videos = onlyReusable(await videoDetails(env, ids), license).slice(0, max);
      return { result: { playlist: playlistId, url: 'https://www.youtube.com/playlist?list=' + playlistId, license: license, videos: videos }, quota: 1 + (ids.length ? 1 : 0) };
    }
  },
  {
    name: 'get_comments',
    description: 'Top-level comments on a video, by relevance or newest first. Costs 1 quota unit.',
    inputSchema: {
      type: 'object',
      properties: {
        video: { type: 'string', description: 'A video id or any YouTube video link.' },
        order: { type: 'string', enum: ['relevance', 'time'], description: 'relevance (default) or time (newest first).' },
        max_results: { type: 'integer', minimum: 1, maximum: 50, description: '1-50 (default 20).' }
      },
      required: ['video']
    },
    async run(args, env) {
      const id = parseVideoId(requireText(args, 'video', 'a video id or link'));
      if (!id) throw new YouTubeError('That is not a YouTube video id or link.');
      const order = args.order || 'relevance';
      if (order !== 'relevance' && order !== 'time') throw new YouTubeError('order must be "relevance" or "time".');
      const body = await youtube(env, 'commentThreads', {
        part: 'snippet',
        videoId: id,
        order: order,
        textFormat: 'plainText',
        maxResults: clampInt(args.max_results, 20, 1, 50)
      });
      return { result: { video: 'https://www.youtube.com/watch?v=' + id, order: order, comments: (body.items || []).map(shapeComment) }, quota: 1 };
    }
  }
];

/* ----------------------------------------------------------------- MCP */

function rpcResult(id, result) {
  return { jsonrpc: '2.0', id: id, result: result };
}

function rpcError(id, code, message) {
  return { jsonrpc: '2.0', id: id === undefined ? null : id, error: { code: code, message: message } };
}

function toolText(ok, value) {
  const text = ok ? JSON.stringify(value, null, 2) : String(value);
  return ok ? { content: [{ type: 'text', text: text }] } : { content: [{ type: 'text', text: text }], isError: true };
}

/* Answer one JSON-RPC message. Returns null for notifications (no id). */
export async function handleRpc(message, env) {
  if (!message || typeof message !== 'object' || Array.isArray(message) || message.jsonrpc !== '2.0' || typeof message.method !== 'string') {
    return rpcError(message && message.id, -32600, 'Not a JSON-RPC 2.0 request');
  }
  const id = message.id;
  const params = message.params || {};
  if (id === undefined) return null; // a notification: nothing to say back
  switch (message.method) {
    case 'initialize': {
      const asked = String(params.protocolVersion || '');
      return rpcResult(id, {
        protocolVersion: PROTOCOL_VERSIONS.includes(asked) ? asked : PROTOCOL_VERSIONS[0],
        capabilities: { tools: { listChanged: false } },
        serverInfo: { name: SERVER_NAME, version: SERVER_VERSION },
        instructions: INSTRUCTIONS
      });
    }
    case 'ping':
      return rpcResult(id, {});
    case 'tools/list':
      return rpcResult(id, { tools: TOOLS.map((t) => ({ name: t.name, description: t.description, inputSchema: t.inputSchema })) });
    case 'tools/call': {
      const tool = TOOLS.find((t) => t.name === params.name);
      if (!tool) return rpcResult(id, toolText(false, 'There is no tool called "' + params.name + '".'));
      const args = params.arguments && typeof params.arguments === 'object' ? params.arguments : {};
      try {
        const out = await tool.run(args, env);
        return rpcResult(id, toolText(true, Object.assign({}, out.result, { quota_used: out.quota })));
      } catch (e) {
        if (e instanceof YouTubeError) return rpcResult(id, toolText(false, e.message));
        return rpcResult(id, toolText(false, 'Something went wrong: ' + (e && e.message || e)));
      }
    }
    default:
      return rpcError(id, -32601, 'Method not found: ' + message.method);
  }
}

/* ---------------------------------------------------------------- HTTP */

function json(status, body) {
  return new Response(body === null ? null : JSON.stringify(body), {
    status: status,
    headers: { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' }
  });
}

/* A secret as stored, minus any stray newline or space a paste or a pipe added. */
function secret(env, name) {
  const v = env && env[name];
  return typeof v === 'string' ? v.trim() : '';
}

function sameSecret(a, b) {
  if (typeof a !== 'string' || typeof b !== 'string' || a.length !== b.length || !a.length) return false;
  let diff = 0;
  for (let i = 0; i < a.length; i++) diff |= a.charCodeAt(i) ^ b.charCodeAt(i);
  return diff === 0;
}

/* The password may come in the path (/mcp/<password>) or as a bearer token. */
function authorized(request, pathPassword, env) {
  const expected = secret(env, 'CONNECTOR_PASSWORD');
  if (!expected) return false;
  let given = pathPassword || '';
  try {
    given = decodeURIComponent(given);
  } catch (e) {
    given = '';
  }
  if (given && sameSecret(given, expected)) return true;
  const auth = request.headers.get('Authorization') || '';
  const m = /^Bearer\s+(.+)$/i.exec(auth);
  return !!(m && sameSecret(m[1].trim(), expected));
}

async function check(env) {
  const out = {
    server: SERVER_NAME + ' ' + SERVER_VERSION,
    api_key_set: !!secret(env, 'YOUTUBE_API_KEY'),
    password_set: !!secret(env, 'CONNECTOR_PASSWORD'),
    tools: TOOLS.map((t) => t.name)
  };
  try {
    const body = await youtube(env, 'videos', { part: 'id', chart: 'mostPopular', maxResults: 1 });
    out.youtube = { ok: Array.isArray(body.items), note: 'YouTube answered. If password_set is true, the connector is ready.' };
  } catch (e) {
    out.youtube = { ok: false, error: String(e && e.message || e) };
  }
  return json(200, out);
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    const path = url.pathname.replace(/\/+$/, '') || '/';

    if (path === '/check' && request.method === 'GET') return check(env);

    const m = /^\/mcp(?:\/([^/]+))?$/.exec(path);
    if (!m) return json(404, { error: 'This is YouTube for Claude. Ask POST /mcp/<password> (the connector) or GET /check.' });

    if (request.method === 'GET') return json(405, { error: 'This server answers POST only; it offers no event stream.' });
    if (request.method === 'DELETE') return new Response(null, { status: 204 });
    if (request.method !== 'POST') return json(405, { error: 'POST only' });
    if (!authorized(request, m[1], env)) {
      return new Response(JSON.stringify({ error: 'Wrong or missing password. The connector URL must be /mcp/<password>.' }), {
        status: 401,
        headers: { 'Content-Type': 'application/json', 'WWW-Authenticate': 'Bearer realm="youtube-for-claude"' }
      });
    }

    let message;
    try {
      message = await request.json();
    } catch (e) {
      return json(400, rpcError(null, -32700, 'Parse error: the body is not JSON'));
    }
    if (Array.isArray(message)) return json(400, rpcError(null, -32600, 'Send one request at a time, not a batch'));

    const answer = await handleRpc(message, env);
    if (answer === null) return new Response(null, { status: 202 });
    return json(200, answer);
  }
};
