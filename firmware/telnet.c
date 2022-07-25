

//MIT License
//
//Copyright (c) 2019 tvelliott
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files (the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions:
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.




#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "telnet.h"

#include "lwip/opt.h"
#include "lwip/tcp.h"
#include "std_io.h"
#include "command.h"
#include "telnet.h"
#include "globals.h"

static struct tcp_pcb *telnet_pcb;
uint8_t cmd_buffer[CMD_BUFFER_LEN];
static int do_close;
static int i;
static int totlen;
static char *ptr;
static struct pbuf *q;
static int len;
static int cmd_idx;
static int tn_send_len;
static int tn_len;
static int is_closed = 1;
static int tn_timeout;
static char *tn_buffer[1576];

int cmd_response_port;

//#define TN_TIMEOUT 2400 //2 minutes @ 50ms tick
//#define ABORT_INUSE

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
int is_telnet_closed()
{
  return ( do_close | is_closed );
}
//////////////////////////////////////////////////////////////////////////////////////////////
// always called regardless of pcb
//////////////////////////////////////////////////////////////////////////////////////////////
void telnet_tick()
{
#ifdef TN_TIMEOUT


  if( ++tn_timeout == TN_TIMEOUT ) {
    close_telnet();
  }
//  else {
//    is_closed=1;
//    printf("\r\ntimeout %d", tn_timeout);
//    is_closed=0;
//  }
#endif
}
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//void telnet_fastpoll( void )
//{
 // telnet_poll( NULL, telnet_pcb );
//}

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
err_t telnet_poll( void *arg, struct tcp_pcb *pcb )
{
  char *tn_ptr = tn_buffer;
  int tn_out;

  if( is_closed ) return -1;

  if( do_close ) {
    do_close = 0;
    tcp_close( pcb );
    return ERR_OK;
  }


  if( pcb != NULL && (netbuf_s!=netbuf_e) ) {

    int avail = tcp_sndbuf( pcb );
    if( avail > 2048 ) {

      tn_out=0;
      while( netbuf_s!=netbuf_e ) {
        *tn_ptr++ = net_buffer[netbuf_s++];
        netbuf_s &= (uint32_t) (MAX_NET_BUFFER-1);
        if(++tn_out>=1460) break;
      }

      if( tn_out > 0 ) {
        telnet_write( pcb, tn_buffer, tn_out );
      }
    }
  }

  return ERR_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
void telnet_write( struct tcp_pcb *tn_write_pcb, uint8_t *buffer, int len )
{


  if( is_closed ) return;
  if( tn_write_pcb == NULL ) return;
  if( len == 0 ) return;

  if( !do_close && len > 0 ) tn_timeout = 0;

  tcp_write( tn_write_pcb, buffer, len, 1 );

}

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
static err_t telnet_recv( void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err )
{

  if(pcb==NULL) return ERR_CONN;  //this probably shouldn't happen
  if(p==NULL) return ERR_CONN;  //this probably shouldn't happen
  if(telnet_pcb==NULL) return ERR_CONN;

  if(pcb->remote_port != telnet_pcb->remote_port) {
    //this is caused by the fact that we always accept new connections and just throw out the old one.
    //If we get here, this is probably one of the old connections.  just reset it by returning ERR_CONN
    if(config->logging>1) printf("\r\nincoming tcp_recv from wrong/old remote_port. resetting the old connection.  %d, %d", pcb->remote_port, telnet_pcb->remote_port);
    if( p ) pbuf_free( p );
    tcp_abort( pcb );
    return ERR_CONN;
  }

  totlen = p->tot_len;
  q = p;
  len = q->len;
  ptr = q->payload;
  cmd_idx = 0;

#ifdef ABORT_INUSE
  if( is_closed ) {
    if( p ) pbuf_free( p );
    tcp_abort( pcb );
    return ERR_CONN;
  }
#endif

  if( p != NULL ) {

    tcp_recved( pcb, p->tot_len );

    while( q ) {
      for( i = 0; i < len; i++ ) {
        cmd_buffer[cmd_idx++] = *ptr++;
        if(cmd_idx>=1500) goto free_pbuf;
      }
      q = q->next;
      if( q != NULL ) {
        len = q->len;
        ptr = q->payload;
      }
    }

    if( !do_close && len > 0 ) tn_timeout = 0;

    //telnet negotiation
    if( cmd_buffer[0] == 0xff ) {

      cmd_buffer[0] = 0xff;  //IAC
      cmd_buffer[1] = 0xfc;  //won't
      cmd_buffer[2] = 0x03;  //echo

      cmd_buffer[3] = 0xff;  //IAC
      cmd_buffer[4] = 0xfe;  //don't
      cmd_buffer[5] = 0x1f;  //negotiate

      cmd_buffer[6] = 0xff;  //IAC
      cmd_buffer[7] = 0xfe;  //don't
      cmd_buffer[8] = 0x22;  //linemode

      telnet_write( pcb, cmd_buffer, 9 );

      memset( cmd_buffer, 0x00, sizeof( cmd_buffer ) );
    } else {
      if( memcmp( cmd_buffer, "quit", 4 ) == 0 || memcmp( cmd_buffer, "exit", 4 ) == 0 ) {
        close_telnet();
        memset( cmd_buffer, 0x00, sizeof( cmd_buffer ) );
      } else {
        for( i = 0; i < cmd_idx; i++ ) {
          if( cmd_buffer[i] == '\r' || cmd_buffer[i] == '\n' ) {
            cmd_response_port = CMD_RESP_TELNET;
            handle_command_telnet( ( char * ) cmd_buffer, 1, INTF_SRC_ETH, cmd_idx, 1 );
            memset( cmd_buffer, 0x00, sizeof( cmd_buffer ) );
            telnet_poll( ( void * ) NULL, pcb );
          }
          if( cmd_buffer[i] == 0x00 ) break;
        }
      }
    }

  }

free_pbuf:
  if(p) pbuf_free( p );

  if( p == NULL || do_close ) {
    do_close = 0;

    //tcp_close(pcb);
    tcp_abort( pcb );

    tcp_arg( pcb, NULL );
    tcp_sent( pcb, NULL );
    tcp_recv( pcb, NULL );
    is_closed = 1;

  }

  return ERR_OK;
}


//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
void handle_command_telnet( char *cmd, int arg2, int interface, int cmd_idx, int arg5 )
{
  parse_command( cmd );

  memset( cmd, 0x00, sizeof( cmd_buffer ) );
}

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
void close_telnet( void )
{
  is_closed = 0;
  do_close = 1;
}

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
void print_prompt( void )
{
  printf( "\r\n~$ " );
}


//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
static err_t telnet_accept( void *arg, struct tcp_pcb *pcb, err_t err )
{
//#ifdef ABORT_INUSE
//  if(!is_closed) return ERR_ISCONN;
//#endif

  LWIP_UNUSED_ARG( arg );
  LWIP_UNUSED_ARG( err );

  tcp_arg( pcb, NULL );
  tcp_sent( pcb, NULL );
  tcp_recv( pcb, telnet_recv );
  tcp_poll( pcb, telnet_poll, 1 );

  cmd_buffer[0] = 0;
  cmd_buffer[1] = 0;
  cmd_buffer[2] = 0;


  is_closed = 0;
  if( !do_close ) tn_timeout = 0;

  uint8_t mcu_letter_version = 'Y';
  if( config->mcu_ver >> 16 == 0x1003 ) mcu_letter_version = 'Y';
  if( config->mcu_ver >> 16 == 0x2001 ) mcu_letter_version = 'X';
  if( config->mcu_ver >> 16 == 0x2003 ) mcu_letter_version = 'V';

  uint32_t clk_mhz = SystemCoreClock / 1e6;
  printf( "\r\nConnected To SuperH+ Running @ %lu MHz (STM32H743 / %c Ver / 0x%04x)", clk_mhz, mcu_letter_version, config->mcu_ver >> 16 );
  int free = _mem_free();
  printf( "\r\nheap mem free %d bytes\r\n", free );
  print_prompt();

  //telnet_poll((void *) NULL, pcb);
  memset( cmd_buffer, 0x00, sizeof( cmd_buffer ) );


  telnet_pcb = pcb;  //we always receive new connections and just reset the previous connection if it sends something.
                     //this behaviour will need to be changed if the device needs to share a network with others


  return ERR_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
void telnet_init( void )
{

  telnet_pcb = tcp_new();
  tcp_bind( telnet_pcb, IP_ADDR_ANY, 23 );
  telnet_pcb = tcp_listen( telnet_pcb );
  tcp_accept( telnet_pcb, telnet_accept );
}
