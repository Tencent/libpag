if (typeof PAG !== 'object') {
    PAG = {};
}

if (typeof JSON === 'undefined') {
    JSON = {
        stringify: function (value) {
            function quote(string) {
                var escapable = /[\\\"\x00-\x1f\x7f-\x9f\u00ad\u0600-\u0604\u070f\u17b4\u17b5\u200c-\u200f\u2028-\u202f\u2060-\u206f\ufeff\ufff0-\uffff]/g;
                var meta = {
                    '\b': '\\b',
                    '\t': '\\t',
                    '\n': '\\n',
                    '\f': '\\f',
                    '\r': '\\r',
                    '"': '\\"',
                    '\\': '\\\\'
                };
                escapable.lastIndex = 0;
                return escapable.test(string) ? '"' + string.replace(escapable, function (a) {
                    var c = meta[a];
                    return typeof c === 'string' ? c : '\\u' + ('0000' + a.charCodeAt(0).toString(16)).slice(-4);
                }) + '"' : '"' + string + '"';
            }

            function str(key, holder) {
                var i, k, v, length, mind = '', partial, value = holder[key];
                switch (typeof value) {
                    case 'string':
                        return quote(value);
                    case 'number':
                        return isFinite(value) ? String(value) : 'null';
                    case 'boolean':
                    case 'null':
                        return String(value);
                    case 'object':
                        if (!value) {
                            return 'null';
                        }
                        if (Object.prototype.toString.apply(value) === '[object Array]') {
                            partial = [];
                            length = value.length;
                            for (i = 0; i < length; i += 1) {
                                partial[i] = str(i, value) || 'null';
                            }
                            v = partial.length === 0 ? '[]' : '[' + partial.join(',') + ']';
                            return v;
                        }
                        partial = [];
                        for (k in value) {
                            if (Object.prototype.hasOwnProperty.call(value, k)) {
                                v = str(k, value);
                                if (v) {
                                    partial.push(quote(k) + ':' + v);
                                }
                            }
                        }
                        return partial.length === 0 ? '{}' : '{' + partial.join(',') + '}';
                }
            }
            return str('', {'': value});
        }
    };
}

(function () {
    'use strict';

    PAG.printTextDocuments = function (compositionID, layerIndex, keyframeIndex) {
        var composition = null;
        for (var i = 1; i <= app.project.numItems; i++) {
            var item = app.project.item(i);
            if (item instanceof CompItem && item.id === compositionID) {
                composition = item;
                break;
            }
        }
        if (composition == null) {
            return "{}";
        }
        if (layerIndex >= composition.layers.length) {
            return "{}";
        }
        var textLayer = composition.layers[layerIndex + 1];
        var sourceText = textLayer.property("Source Text");
        if (!sourceText) {
            return "{}";
        }
        var textDocument;
        if (keyframeIndex === 0 && sourceText.numKeys === 0) {
            textDocument = sourceText.value;
        } else {
            textDocument = sourceText.keyValue(keyframeIndex + 1);
        }
        if (!textDocument) {
            return "{}";
        }
        var resultObject = {};
        for (var key in textDocument) {
            if (!Object.prototype.hasOwnProperty.call(textDocument, key)) {
                continue;
            }
            try {
                var value = textDocument[key];
            } catch (e) {
                continue;
            }
            switch (typeof value) {
                case 'string':
                    value = value.split("\x03").join("\n");
                    value = value.split("\r\n").join("\n");
                    value = value.split("\r").join("\n");
                    value = value.split("\n").join("\\n");
                    resultObject[key] = value;
                    break;
                case 'number':
                case 'boolean':
                    resultObject[key] = value;
                    break;
                case 'object':
                    if (value && Object.prototype.toString.apply(value) === '[object Array]') {
                        var partial = [];
                        var length = value.length;
                        for (var i = 0; i < length; i += 1) {
                            partial[i] = String(value[i]);
                        }
                        resultObject[key] = partial;
                    }
                    break;
            }
        }
        return JSON.stringify(resultObject);
    }
}());
