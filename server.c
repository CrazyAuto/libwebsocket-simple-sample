#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>

#define WS_LOG_ERROR(fmt,args...) printf("%s(%d)-%s -> " #fmt "\n", __FILE__, __LINE__, __FUNCTION__, ##args);
#define WS_LOG_DEBUG(fmt,args...) printf("%s(%d)-%s -> " #fmt "\n", __FILE__, __LINE__, __FUNCTION__, ##args);
#define EXAMPLE_RX_BUFFER_BYTES (100)

enum protocols
{
	PROTOCOL_HTTP = 0,
	PROTOCOL_EXAMPLE,
	PROTOCOL_COUNT
};

struct payload
{
	unsigned char data[LWS_SEND_BUFFER_PRE_PADDING + EXAMPLE_RX_BUFFER_BYTES + LWS_SEND_BUFFER_POST_PADDING];
	size_t len;
} received_payload;


static int callback_http( struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len )
{
	switch( reason )
	{
		case LWS_CALLBACK_HTTP:
			lws_serve_http_file( wsi, "example.html", "text/html", NULL, 0 );
			break;
		default:
			break;
	}

	return 0;
}

static int callback_example( struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len )
{
	switch( reason )
	{
		case LWS_CALLBACK_RECEIVE:
            WS_LOG_DEBUG("server recive %s",(char *)in)
			memcpy( &received_payload.data[LWS_SEND_BUFFER_PRE_PADDING], in, len );
			received_payload.len = len;
			lws_callback_on_writable_all_protocol( lws_get_context( wsi ), lws_get_protocol( wsi ) );
			break;

		case LWS_CALLBACK_SERVER_WRITEABLE:
            WS_LOG_DEBUG("server write %s",(char *)in);
			lws_write( wsi, &received_payload.data[LWS_SEND_BUFFER_PRE_PADDING], received_payload.len, LWS_WRITE_TEXT );
			break;

		default:
			break;
	}

	return 0;
}

static struct lws_protocols protocols[] =
{
	/* The first protocol must always be the HTTP handler */
	{
		"http-only",   /* name */
		callback_http, /* callback */
		0,             /* No per session data. */
		0,             /* max frame size / rx buffer */
	},
	{
		"example-protocol",
		callback_example,
		0,
		EXAMPLE_RX_BUFFER_BYTES,
	},
	{ NULL, NULL, 0, 0 } /* terminator */
};

int main( int argc, char *argv[] )
{
	struct lws_context_creation_info info;
	memset( &info, 0, sizeof(info) );

	info.port = 8000;
	info.protocols = protocols;
	info.gid = -1;
	info.uid = -1;

	struct lws_context *context = lws_create_context( &info );

	while( 1 )
	{
		lws_service( context, /* timeout_ms = */ 0 );
	}

	lws_context_destroy( context );

	return 0;
}
