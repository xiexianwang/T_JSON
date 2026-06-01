(function (global, factory) {
	typeof exports === 'object' && typeof module !== 'undefined' ? factory(require('leaflet')) :
	typeof define === 'function' && define.amd ? define(['leaflet'], factory) :
	(factory(global.L));
}(this, (function (L) { 'use strict';

L = L && L.hasOwnProperty('default') ? L['default'] : L;

function pointDistance(ptA, ptB) {
    var x = ptB.x - ptA.x;
    var y = ptB.y - ptA.y;
    return Math.sqrt(x * x + y * y);
}

var computeSegmentHeading = function computeSegmentHeading(a, b) {
    return (Math.atan2(b.y - a.y, b.x - a.x) * 180 / Math.PI + 90 + 360) % 360;
};

var asRatioToPathLength = function asRatioToPathLength(_ref, totalPathLength) {
    var value = _ref.value, isInPixels = _ref.isInPixels;
    return isInPixels ? value / totalPathLength : value;
};

function parseRelativeOrAbsoluteValue(value) {
    if (typeof value === 'string' && value.indexOf('%') !== -1) {
        return { value: parseFloat(value) / 100, isInPixels: false };
    }
    var parsedValue = value ? parseFloat(value) : 0;
    return { value: parsedValue, isInPixels: parsedValue > 0 };
}

var pointsEqual = function pointsEqual(a, b) {
    return a.x === b.x && a.y === b.y;
};

function pointsToSegments(pts) {
    return pts.reduce(function (segments, b, idx, points) {
        if (idx > 0 && !pointsEqual(b, points[idx - 1])) {
            var a = points[idx - 1];
            var distA = segments.length > 0 ? segments[segments.length - 1].distB : 0;
            var distAB = pointDistance(a, b);
            segments.push({
                a: a, b: b, distA: distA, distB: distA + distAB,
                heading: computeSegmentHeading(a, b)
            });
        }
        return segments;
    }, []);
}

function projectPatternOnPointPath(pts, pattern) {
    var segments = pointsToSegments(pts);
    var nbSegments = segments.length;
    if (nbSegments === 0) return [];

    var totalPathLength = segments[nbSegments - 1].distB;
    var offset = asRatioToPathLength(pattern.offset, totalPathLength);
    var endOffset = asRatioToPathLength(pattern.endOffset, totalPathLength);
    var repeat = asRatioToPathLength(pattern.repeat, totalPathLength);
    var repeatIntervalPixels = totalPathLength * repeat;
    var startOffsetPixels = offset > 0 ? totalPathLength * offset : 0;
    var endOffsetPixels = endOffset > 0 ? totalPathLength * endOffset : 0;

    var positionOffsets = [];
    var positionOffset = startOffsetPixels;
    do {
        positionOffsets.push(positionOffset);
        positionOffset += repeatIntervalPixels;
    } while (repeatIntervalPixels > 0 && positionOffset < totalPathLength - endOffsetPixels);

    var segmentIndex = 0;
    var segment = segments[0];
    return positionOffsets.map(function (positionOffset) {
        while (positionOffset > segment.distB && segmentIndex < nbSegments - 1) {
            segmentIndex++;
            segment = segments[segmentIndex];
        }
        var segmentRatio = (positionOffset - segment.distA) / (segment.distB - segment.distA);
        return { pt: interpolateBetweenPoints(segment.a, segment.b, segmentRatio), heading: segment.heading };
    });
}

function interpolateBetweenPoints(ptA, ptB, ratio) {
    if (ptB.x !== ptA.x) {
        return { x: ptA.x + ratio * (ptB.x - ptA.x), y: ptA.y + ratio * (ptB.y - ptA.y) };
    }
    return { x: ptA.x, y: ptA.y + (ptB.y - ptA.y) * ratio };
}

(function() {
    var proto_initIcon = L.Marker.prototype._initIcon;
    var proto_setPos = L.Marker.prototype._setPos;
    var oldIE = (L.DomUtil.TRANSFORM === 'msTransform');

    L.Marker.addInitHook(function () {
        var iconOptions = this.options.icon && this.options.icon.options;
        var iconAnchor = iconOptions && this.options.icon.options.iconAnchor;
        if (iconAnchor) {
            iconAnchor = (iconAnchor[0] + 'px ' + iconAnchor[1] + 'px');
        }
        this.options.rotationOrigin = this.options.rotationOrigin || iconAnchor || 'center bottom';
        this.options.rotationAngle = this.options.rotationAngle || 0;
        this.on('drag', function(e) { e.target._applyRotation(); });
    });

    L.Marker.include({
        _initIcon: function() { proto_initIcon.call(this); },
        _setPos: function (pos) { proto_setPos.call(this, pos); this._applyRotation(); },
        _applyRotation: function () {
            if (this.options.rotationAngle) {
                this._icon.style[L.DomUtil.TRANSFORM+'Origin'] = this.options.rotationOrigin;
                if (oldIE) {
                    this._icon.style[L.DomUtil.TRANSFORM] = 'rotate(' + this.options.rotationAngle + 'deg)';
                } else {
                    this._icon.style[L.DomUtil.TRANSFORM] += ' rotateZ(' + this.options.rotationAngle + 'deg)';
                }
            }
        },
        setRotationAngle: function(angle) { this.options.rotationAngle = angle; this.update(); return this; },
        setRotationOrigin: function(origin) { this.options.rotationOrigin = origin; this.update(); return this; }
    });
})();

L.Symbol = L.Symbol || {};

L.Symbol.Dash = L.Class.extend({
    options: { pixelSize: 10, pathOptions: {} },
    initialize: function initialize(options) {
        L.Util.setOptions(this, options);
        this.options.pathOptions.clickable = false;
    },
    buildSymbol: function buildSymbol(dirPoint, latLngs, map, index, total) {
        var opts = this.options;
        var d2r = Math.PI / 180;
        if (opts.pixelSize <= 1) {
            return L.polyline([dirPoint.latLng, dirPoint.latLng], opts.pathOptions);
        }
        var midPoint = map.project(dirPoint.latLng);
        var angle = -(dirPoint.heading - 90) * d2r;
        var a = L.point(midPoint.x + opts.pixelSize * Math.cos(angle + Math.PI) / 2, midPoint.y + opts.pixelSize * Math.sin(angle) / 2);
        var b = midPoint.add(midPoint.subtract(a));
        return L.polyline([map.unproject(a), map.unproject(b)], opts.pathOptions);
    }
});
L.Symbol.dash = function (options) { return new L.Symbol.Dash(options); };

L.Symbol.ArrowHead = L.Class.extend({
    options: { polygon: true, pixelSize: 10, headAngle: 60, pathOptions: { stroke: false, weight: 2 } },
    initialize: function initialize(options) {
        L.Util.setOptions(this, options);
        this.options.pathOptions.clickable = false;
    },
    buildSymbol: function buildSymbol(dirPoint, latLngs, map, index, total) {
        return this.options.polygon
            ? L.polygon(this._buildArrowPath(dirPoint, map), this.options.pathOptions)
            : L.polyline(this._buildArrowPath(dirPoint, map), this.options.pathOptions);
    },
    _buildArrowPath: function _buildArrowPath(dirPoint, map) {
        var d2r = Math.PI / 180;
        var tipPoint = map.project(dirPoint.latLng);
        var direction = -(dirPoint.heading - 90) * d2r;
        var radianArrowAngle = this.options.headAngle / 2 * d2r;
        var headAngle1 = direction + radianArrowAngle;
        var headAngle2 = direction - radianArrowAngle;
        var arrowHead1 = L.point(tipPoint.x - this.options.pixelSize * Math.cos(headAngle1), tipPoint.y + this.options.pixelSize * Math.sin(headAngle1));
        var arrowHead2 = L.point(tipPoint.x - this.options.pixelSize * Math.cos(headAngle2), tipPoint.y + this.options.pixelSize * Math.sin(headAngle2));
        return [map.unproject(arrowHead1), dirPoint.latLng, map.unproject(arrowHead2)];
    }
});
L.Symbol.arrowHead = function (options) { return new L.Symbol.ArrowHead(options); };

L.Symbol.Marker = L.Class.extend({
    options: { markerOptions: {}, rotate: false },
    initialize: function initialize(options) {
        L.Util.setOptions(this, options);
        this.options.markerOptions.clickable = false;
        this.options.markerOptions.draggable = false;
    },
    buildSymbol: function buildSymbol(directionPoint, latLngs, map, index, total) {
        if (this.options.rotate) {
            this.options.markerOptions.rotationAngle = directionPoint.heading + (this.options.angleCorrection || 0);
        }
        return L.marker(directionPoint.latLng, this.options.markerOptions);
    }
});
L.Symbol.marker = function (options) { return new L.Symbol.Marker(options); };

var isCoord = function isCoord(c) {
    return c instanceof L.LatLng || Array.isArray(c) && c.length === 2 && typeof c[0] === 'number';
};
var isCoordArray = function isCoordArray(ll) {
    return Array.isArray(ll) && isCoord(ll[0]);
};

L.PolylineDecorator = L.FeatureGroup.extend({
    options: { patterns: [] },
    initialize: function initialize(paths, options) {
        L.FeatureGroup.prototype.initialize.call(this);
        L.Util.setOptions(this, options);
        this._map = null;
        this._paths = this._initPaths(paths);
        this._bounds = this._initBounds();
        this._patterns = this._initPatterns(this.options.patterns);
    },
    _initPaths: function _initPaths(input, isPolygon) {
        var _this = this;
        if (isCoordArray(input)) {
            var coords = isPolygon ? input.concat([input[0]]) : input;
            return [coords];
        }
        if (input instanceof L.Polyline) {
            return this._initPaths(input.getLatLngs(), input instanceof L.Polygon);
        }
        if (Array.isArray(input)) {
            return input.reduce(function (flatArray, p) {
                return flatArray.concat(_this._initPaths(p, isPolygon));
            }, []);
        }
        return [];
    },
    _initPatterns: function _initPatterns(patternDefs) {
        return patternDefs.map(this._parsePatternDef);
    },
    setPatterns: function setPatterns(patterns) {
        this.options.patterns = patterns;
        this._patterns = this._initPatterns(this.options.patterns);
        this.redraw();
    },
    setPaths: function setPaths(paths) {
        this._paths = this._initPaths(paths);
        this._bounds = this._initBounds();
        this.redraw();
    },
    _parsePatternDef: function _parsePatternDef(patternDef) {
        return {
            symbolFactory: patternDef.symbol,
            offset: parseRelativeOrAbsoluteValue(patternDef.offset),
            endOffset: parseRelativeOrAbsoluteValue(patternDef.endOffset),
            repeat: parseRelativeOrAbsoluteValue(patternDef.repeat)
        };
    },
    onAdd: function onAdd(map) {
        this._map = map;
        this._draw();
        this._map.on('moveend', this.redraw, this);
    },
    onRemove: function onRemove(map) {
        this._map.off('moveend', this.redraw, this);
        this._map = null;
        L.FeatureGroup.prototype.onRemove.call(this, map);
    },
    _initBounds: function _initBounds() {
        var allPathCoords = this._paths.reduce(function (acc, path) { return acc.concat(path); }, []);
        return L.latLngBounds(allPathCoords);
    },
    getBounds: function getBounds() { return this._bounds; },
    _buildSymbols: function _buildSymbols(latLngs, symbolFactory, directionPoints) {
        var _this2 = this;
        return directionPoints.map(function (directionPoint, i) {
            return symbolFactory.buildSymbol(directionPoint, latLngs, _this2._map, i, directionPoints.length);
        });
    },
    _getDirectionPoints: function _getDirectionPoints(latLngs, pattern) {
        var _this3 = this;
        if (latLngs.length < 2) return [];
        var pathAsPoints = latLngs.map(function (latLng) { return _this3._map.project(latLng); });
        return projectPatternOnPointPath(pathAsPoints, pattern).map(function (point) {
            return { latLng: _this3._map.unproject(L.point(point.pt)), heading: point.heading };
        });
    },
    redraw: function redraw() {
        if (!this._map) return;
        this.clearLayers();
        this._draw();
    },
    _getPatternLayers: function _getPatternLayers(pattern) {
        var _this4 = this;
        var mapBounds = this._map.getBounds().pad(0.1);
        return this._paths.map(function (path) {
            var directionPoints = _this4._getDirectionPoints(path, pattern)
                .filter(function (point) { return mapBounds.contains(point.latLng); });
            return L.featureGroup(_this4._buildSymbols(path, pattern.symbolFactory, directionPoints));
        });
    },
    _draw: function _draw() {
        var _this5 = this;
        this._patterns.map(function (pattern) { return _this5._getPatternLayers(pattern); })
            .forEach(function (layers) { _this5.addLayer(L.featureGroup(layers)); });
    }
});
L.polylineDecorator = function (paths, options) {
    return new L.PolylineDecorator(paths, options);
};

})));
