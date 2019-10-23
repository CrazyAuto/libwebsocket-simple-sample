#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>

pthread_t read_from_pipe_thread;
static struct lws *web_socket = NULL;
static int websocket_rx;
static int websocket_tx;
char buffer_from_fifo[256]={0};


#define EXAMPLE_RX_BUFFER_BYTES (100)
#define WEBSOCKET_RX "/tmp/websocket_rx"
#define WEBSOCKET_TX "/tmp/websocket_tx"
#define WS_LOG_ERROR(fmt,args...) printf("%s(%d)-%s -> " #fmt "\n", __FILE__, __LINE__, __FUNCTION__, ##args);
#define WS_LOG_DEBUG(fmt,args...) printf("%s(%d)-%s -> " #fmt "\n", __FILE__, __LINE__, __FUNCTION__, ##args);

static int callback_example( struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len )
{
	switch( reason )
	{
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			lws_callback_on_writable( wsi );
			break;

		case LWS_CALLBACK_CLIENT_RECEIVE:
			/* Handle incomming messages here. */
            WS_LOG_DEBUG("message incoming,%s",(char *)in);
            if(write(websocket_rx,in,len)<=0)
            {
                WS_LOG_DEBUG("some error happens when write to fifo");
            }
			break;

		case LWS_CALLBACK_CLIENT_WRITEABLE:
		{
            WS_LOG_DEBUG("write message to server");
			unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + EXAMPLE_RX_BUFFER_BYTES + LWS_SEND_BUFFER_POST_PADDING];
			unsigned char *p = &buf[LWS_SEND_BUFFER_PRE_PADDING];
			//size_t n = sprintf( (char *)p, "hello world %d", counter++ );
			if( strlen(buffer_from_fifo) > EXAMPLE_RX_BUFFER_BYTES )
            {
                printf("buffer from fifo is too long\n");
                break;
            }
			memcpy((char *)p,buffer_from_fifo,strlen(buffer_from_fifo));
			lws_write( wsi, p, strlen(buffer_from_fifo), LWS_WRITE_TEXT );
			break;
		}

		case LWS_CALLBACK_CLOSED:
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			web_socket = NULL;
			break;

		default:
			break;
	}

	return 0;
}

enum protocols
{
	PROTOCOL_EXAMPLE = 0,
	PROTOCOL_COUNT
};

static struct lws_protocols protocols[] =
{
	{
		"example-protocol",
		callback_example,
		0,
		EXAMPLE_RX_BUFFER_BYTES,
	},
	{ NULL, NULL, 0, 0 } /* terminator */
};

static void SignalHandler(int nSigno)
{
    switch(nSigno)
    {
        case SIGPIPE:
    	    WS_LOG_ERROR("SIGPIPE, process go on");
            break;
        default:
            WS_LOG_DEBUG("%d signal unregister\n", nSigno);
            break;
    }
}


static void InitSignalHandler()
{
    signal(SIGPIPE , &SignalHandler);
}


int create_fifo(char *fifo_name)
{
    //const char *fifo_name = "/tmp/websocket_tx";
    int ret = 0;
    int fifo_fd;

    if(access(fifo_name, F_OK) == -1)
    {
        WS_LOG_DEBUG("Create the fifo pipe.\n");
        ret = mkfifo(fifo_name, 0666);
        if(ret != 0)
        {
            WS_LOG_DEBUG("Could not create fifo\n");
            return -1;
        }
    }

    WS_LOG_DEBUG("create fifo OK\n");

    fifo_fd = open(fifo_name, O_RDWR);
    if(fifo_fd <= 0)
    {
        WS_LOG_DEBUG("!!!!!!!open fifo FAIL\n");
        return -1;
    }
    else
    {
        WS_LOG_DEBUG("!!!!open fifo SUCCESS\n");
        //return 0;
    }
    //InitSignalHandler();
    return fifo_fd;
}


static void read_from_pipe_thread_func(void)
{
    int iret = 0;
    struct timeval tv;
    fd_set readfds;

    memset(&tv, 0, sizeof(struct timeval));
    FD_ZERO(&readfds);
    FD_SET(websocket_tx,&readfds);

    while(1)
    {
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        iret = select(websocket_tx+ 1, &readfds, NULL, NULL, &tv);
        if (0 >= iret)
        {
            WS_LOG_DEBUG("select  timeout,errno:%d\n",errno);
            continue;
        }
        else
        {
            if(FD_ISSET(websocket_tx, &readfds))
            {
                if(read(websocket_tx,buffer_from_fifo,sizeof(buffer_from_fifo)) > 0)
                {
                     WS_LOG_DEBUG("read from fifo %s",buffer_from_fifo);
                     if(web_socket)
                     {
                        WS_LOG_ERROR("web socket not init");
                        lws_callback_on_writable( web_socket );
                     }
                }
                else
                    WS_LOG_DEBUG("read error");
            }
            else
                WS_LOG_DEBUG("fifo fd is not in readfds");
        }
    }
}

int main( int argc, char *argv[] )
{
    int ret=-1;
	struct lws_context_creation_info info;
	memset( &info, 0, sizeof(info) );

	info.port = CONTEXT_PORT_NO_LISTEN;
	info.protocols = protocols;
	info.gid = -1;
	info.uid = -1;

	struct lws_context *context = lws_create_context( &info );

	if( !web_socket )
	{
		struct lws_client_connect_info ccinfo = {0};
		ccinfo.context = context;
		ccinfo.address = "localhost";
		ccinfo.port = 8000;
		ccinfo.path = "/";
		ccinfo.host = lws_canonical_hostname( context );
		ccinfo.origin = "origin";
		ccinfo.protocol = protocols[PROTOCOL_EXAMPLE].name;
		web_socket = lws_client_connect_via_info(&ccinfo);
	}
    websocket_rx = create_fifo(WEBSOCKET_RX);
    websocket_tx = create_fifo(WEBSOCKET_TX);

    InitSignalHandler();

    ret = pthread_create(&read_from_pipe_thread, NULL, (void*)&read_from_pipe_thread_func, NULL);
    if(ret)
    {
        WS_LOG_DEBUG("create thread error,error:%d,errno:%d",ret,errno);
    }
    while( 1 )
	{
    	if( !web_socket )
    	{
    		struct lws_client_connect_info ccinfo = {0};
    		ccinfo.context = context;
    		ccinfo.address = "localhost";
    		ccinfo.port = 8000;
    		ccinfo.path = "/";
    		ccinfo.host = lws_canonical_hostname( context );
    		ccinfo.origin = "origin";
    		ccinfo.protocol = protocols[PROTOCOL_EXAMPLE].name;
    		web_socket = lws_client_connect_via_info(&ccinfo);
    	}
       // lws_callback_on_writable( web_socket );
		lws_service( context, 100000);
	}

	lws_context_destroy( context );

	return 0;
}
