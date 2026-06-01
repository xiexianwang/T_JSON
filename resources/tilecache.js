/**
 * tilecache.js - 基于 IndexedDB 的 Leaflet 瓦片缓存
 *
 * 用法：L.tileLayer.cached(url, { layerId: 'vec', ... })
 * 优先从 IndexedDB 读取，命中失败时下载并写入缓存。
 * 适用于天地图等需要减少重复请求的场景。
 */
(function () {
    var DB_NAME = 'TJsonMapTiles';
    var DB_VERSION = 1;
    var STORE = 'tiles';
    var MAX_AGE_MS = 30 * 24 * 60 * 60 * 1000; // 30 天过期
    var dbPromise = null;

    function openDB() {
        if (dbPromise) return dbPromise;
        if (typeof indexedDB === 'undefined') {
            dbPromise = Promise.reject(new Error('IndexedDB unavailable'));
            return dbPromise;
        }
        dbPromise = new Promise(function (resolve, reject) {
            var req = indexedDB.open(DB_NAME, DB_VERSION);
            req.onupgradeneeded = function (e) {
                var db = e.target.result;
                if (!db.objectStoreNames.contains(STORE)) {
                    db.createObjectStore(STORE, { keyPath: 'key' });
                }
            };
            req.onsuccess = function (e) { resolve(e.target.result); };
            req.onerror = function (e) { reject(e); };
        });
        return dbPromise;
    }

    function getTile(key) {
        return openDB().then(function (db) {
            return new Promise(function (resolve) {
                var tx = db.transaction(STORE, 'readonly');
                var req = tx.objectStore(STORE).get(key);
                req.onsuccess = function () {
                    var r = req.result;
                    if (r && (Date.now() - r.ts) < MAX_AGE_MS) {
                        resolve(r.blob);
                    } else {
                        resolve(null);
                    }
                };
                req.onerror = function () { resolve(null); };
            });
        }).catch(function () { return null; });
    }

    function putTile(key, blob) {
        return openDB().then(function (db) {
            return new Promise(function (resolve) {
                var tx = db.transaction(STORE, 'readwrite');
                tx.objectStore(STORE).put({ key: key, blob: blob, ts: Date.now() });
                tx.oncomplete = function () { resolve(); };
                tx.onerror = function () { resolve(); };
            });
        }).catch(function () { });
    }

    function clearAll() {
        return openDB().then(function (db) {
            return new Promise(function (resolve) {
                var tx = db.transaction(STORE, 'readwrite');
                tx.objectStore(STORE).clear();
                tx.oncomplete = function () { resolve(); };
                tx.onerror = function () { resolve(); };
            });
        }).catch(function () { });
    }

    L.TileLayer.Cached = L.TileLayer.extend({
        createTile: function (coords, done) {
            var tile = document.createElement('img');
            tile.alt = '';
            tile.setAttribute('role', 'presentation');

            var url = this.getTileUrl(coords);
            var layerId = this.options.layerId || 'default';
            var key = layerId + '/' + coords.z + '/' + coords.x + '/' + coords.y;

            getTile(key).then(function (blob) {
                if (blob) {
                    tile.src = URL.createObjectURL(blob);
                    done(null, tile);
                } else {
                    tile.onload = function () {
                        fetch(url, { mode: 'cors' })
                            .then(function (r) { return r.blob(); })
                            .then(function (b) { putTile(key, b); })
                            .catch(function () { });
                        done(null, tile);
                    };
                    tile.onerror = function () { done(new Error('Tile load failed'), tile); };
                    tile.src = url;
                }
            });

            return tile;
        }
    });

    L.tileLayer.cached = function (url, options) {
        return new L.TileLayer.Cached(url, options || {});
    };

    window.TileCache = { get: getTile, put: putTile, clear: clearAll };
})();
