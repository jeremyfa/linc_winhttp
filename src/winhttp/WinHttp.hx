package winhttp;

import haxe.ds.StringMap;
import haxe.io.Bytes;

using StringTools;

enum abstract WinHttpMethod(Int) from Int to Int {
    public var GET = 0;
    public var POST = 1;
    public var PUT = 2;
    public var DELETE = 3;
}

typedef WinHttpResponse = {

    public var status:Int;

    public var headers:Map<String,String>;

    public var content:String;

    public var error:String;

}

@:keep
@:keepSub
class WinHttp {

    public static function sendHttpRequest(requestId:Int, url:String, method:WinHttpMethod, body:String, headers:Map<String,String>, proxy:String, timeout:Int):WinHttpResponse {

        var domain = "";
        var port = 80;
        var https = false;
        var path = "/";

        var regex = ~/^(https?):\/\/([^\/:]+)(?::(\d+))?(\/.*)?$/;
        if (regex.match(url)) {
            https = regex.matched(1) == "https";
            domain = regex.matched(2);
            if (regex.matched(3) != null) {
                port = Std.parseInt(regex.matched(3));
            } else {
                port = https ? 443 : 80;
            }
            if (regex.matched(4) != null && regex.matched(4) != "") {
                path = regex.matched(4);
            }
        } else {
            throw "Invalid URL: " + url;
        }

        var rawHeaders = new StringBuf();
        if (headers != null) {
            for (key => val in headers) {
                rawHeaders.add(key);
                rawHeaders.addChar(':'.code);
                rawHeaders.addChar(' '.code);
                rawHeaders.add(val);
                rawHeaders.addChar('\r'.code);
                rawHeaders.addChar('\n'.code);
            }
        }

        final rawResponse:Dynamic = WinHttp_Extern.sendHttpRequest(requestId, domain, port, https, path, method, body, rawHeaders.toString(), proxy, timeout);

        final responseHeaders = new Map<String,String>();
        final rawResponseHeaders:String = rawResponse.headers;
        var hasContentLength:Bool = false;
        if (rawResponseHeaders != null) {
            for (line in rawResponseHeaders.split('\n')) {
                var trimmed = line.trim();
                if (trimmed.length > 0) {
                    var idx = trimmed.indexOf(":");
                    if (idx > 0) {
                        var key = trimmed.substr(0, idx).trim();
                        if (key.toLowerCase() == 'content-length') {
                            hasContentLength = true;
                        }
                        var value = trimmed.substr(idx + 1).trim();
                        responseHeaders.set(key, value);
                    }
                }
            }
        }

        if (!hasContentLength) {
            responseHeaders.set("Content-Length", ""+rawResponse.contentLength);
        }

        final response:WinHttpResponse = {
            status: rawResponse.status,
            headers: responseHeaders,
            content: rawResponse.text,
            error: rawResponse.error
        };

        return response;
    }

}

@:keep
#if !display
@:build(linc.Linc.touch())
@:build(linc.Linc.xml('winhttp'))
#end
@:include('linc_winhttp.h')
extern class WinHttp_Extern {

    @:native('::linc::winhttp::sendHttpRequest')
    static function sendHttpRequest(requestId:Int, domain:String, port:Int, https:Bool, path:String, method:Int, body:String, headers:String, proxy:String, timeout:Int):Dynamic;

}
