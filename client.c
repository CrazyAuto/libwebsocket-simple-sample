#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

pthread_t read_from_pipe_thread;
static struct lws *web_socket = NULL;
static int counter=0;
static int fifo_fd = -1;
char buffer_from_fifo[256]={0};


#define EXAMPLE_RX_BUFFER_BYTES (20)
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
            printf("message incoming,%s",(char *)in);
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


int create_fifo(void)
{
    const char *fifo_name = "/tmp/websocket";
    int ret = 0;

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
        return 0;
    }

    InitSignalHandler();
}


static void read_from_pipe_thread_func(void)
{
    int iret = 0;
    struct timeval tv;
    fd_set readfds;

    memset(&tv, 0, sizeof(struct timeval));
    FD_ZERO(&readfds);
    FD_SET(fifo_fd,&readfds);

    while(1)
    {
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        iret = select(fifo_fd + 1, &readfds, NULL, NULL, &tv);
        if (0 >= iret)
        {
            WS_LOG_DEBUG("select  timeout,errno:%d\n",errno);
            continue;
        }
        else
        {
            if(FD_ISSET(fifo_fd, &readfds))
            {
                if(read(fifo_fd,buffer_from_fifo,sizeof(buffer_from_fifo)) > 0)
                {
                     WS_LOG_DEBUG("read from fifo %s",buffer_from_fifo);
                     lws_callback_on_writable( web_socket );
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
    create_fifo();
    if(!pthread_create(&read_from_pipe_thread, NULL, (void*)&read_from_pipe_thread_func, NULL))
    {
        WS_LOG_DEBUG("create thread error");
    }
    while( 1 )
	{
       // lws_callback_on_writable( web_socket );
		lws_service( context, 100000);
	}

	lws_context_destroy( context );

	return 0;
}
