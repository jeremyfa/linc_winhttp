
import winhttp.WinHttp;

function main() {

    final response = WinHttp.sendHttpRequest(1, "https://haxe.org", GET, null, null, null, 30);

    for (key => val in response.headers) {
        trace('$key: $val');
    }

    trace(response.status);

}
