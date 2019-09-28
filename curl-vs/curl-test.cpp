#include <stdio.h>
#include <string.h>
#include <curl/curl.h>

#ifdef _WIN32
#include <windows.h>
#define WAITMS(x) Sleep(x)
#else
#include <sys/time.h>
#include <unistd.h>
/* Portable sleep for platforms other than Windows. */
#define WAITMS(x)                               \
  do { struct timeval wait = { 0, (x) * 1000 };  \
  (void)select(0, NULL, NULL, NULL, &wait); }while(0)
#endif

const char wsconnect[] = "GET /chat HTTP/1.1\r\n"
"Host: 127.0.0.1\r\n"
"Upgrade: websocket\r\n"
"Connection: Upgrade\r\n"
"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
"Sec-WebSocket-Protocol: chat, superchat\r\n"
"Sec-WebSocket-Version: 13\r\n"
"\r\n";


#define MAX_CONNECTIONS 3
#define USE_FORBID_REUSE 0

int main(void)
{
    CURL *http_handle;
    CURLM *multi_handle;
    CURLMsg* msg;
    int msgsInQueue = 0;
    int still_running = 1; /* keep number of running handles */

    curl_global_init(CURL_GLOBAL_DEFAULT);
    multi_handle = curl_multi_init();
    curl_multi_setopt(multi_handle, CURLMOPT_MAXCONNECTS, MAX_CONNECTIONS);

    for (int i = 0; i < 3; ++i)
    {
        http_handle = curl_easy_init();
        curl_easy_setopt(http_handle, CURLOPT_URL, "http://127.0.0.1:1122/");
        curl_easy_setopt(http_handle, CURLOPT_CONNECT_ONLY, 1L);
        curl_easy_setopt(http_handle, CURLOPT_MAXCONNECTS, 1L);
        curl_easy_setopt(http_handle, CURLOPT_FRESH_CONNECT, 1L);
#if USE_FORBID_REUSE
        curl_easy_setopt(http_handle, CURLOPT_FORBID_REUSE, 1L);
#endif

        /* add the individual transfers */
        curl_multi_add_handle(multi_handle, http_handle);

        curl_multi_perform(multi_handle, &still_running);
        Sleep(300);

        while (msg = curl_multi_info_read(multi_handle, &msgsInQueue))
        {
            if (msg->msg == CURLMSG_DONE)
            {
                CURLcode result = msg->data.result;
                CURL *curl = msg->easy_handle;

                curl_socket_t sockWs;
                {
                    CURLcode ret = curl_easy_getinfo(http_handle, CURLINFO_ACTIVESOCKET, &sockWs);
                    printf("CURLINFO_ACTIVESOCKET, ret:%d sockWs:%d\n", ret, (int)sockWs);
                }

                {
                    size_t sentSize = 0;
                    CURLcode ret = curl_easy_send(http_handle, wsconnect, sizeof(wsconnect), &sentSize);
                    printf("curl_easy_send ret:%d sentSize:%zu\n", ret, sentSize);
                }

                Sleep(300);

                {
                    char buf[1000];
                    size_t recvSize = 0;
                    CURLcode ret = curl_easy_recv(http_handle, buf, sizeof(buf), &recvSize);
                    printf("curl_easy_recv ret:%d recvSize:%zu\n", ret, recvSize);
                }
                break;
            }
        }

        curl_multi_remove_handle(multi_handle, http_handle);
        curl_easy_cleanup(http_handle);
    }

    curl_multi_cleanup(multi_handle);
    curl_global_cleanup();
    return 0;
}
