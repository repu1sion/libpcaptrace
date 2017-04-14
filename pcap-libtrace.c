
//something from libpcap
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libtrace.h>

//METHODS FROM LIBPCAP (function pointers inside struct pcap) - SHOULD BE 12.
#if 0
read_op_t read_op; - Method to call to read packets on a live capture.
int (*next_packet_op)(pcap_t *, struct pcap_pkthdr *, u_char **); read packets from a savefile.
typedef int     (*activate_op_t)(pcap_t *);
typedef int     (*can_set_rfmon_op_t)(pcap_t *);
typedef int     (*inject_op_t)(pcap_t *, const void *, size_t);
typedef int     (*setfilter_op_t)(pcap_t *, struct bpf_program *);
typedef int     (*setdirection_op_t)(pcap_t *, pcap_direction_t);
typedef int     (*set_datalink_op_t)(pcap_t *, int);
typedef int     (*getnonblock_op_t)(pcap_t *, char *);
typedef int     (*setnonblock_op_t)(pcap_t *, int, char *);
typedef int     (*stats_op_t)(pcap_t *, struct pcap_stat *);
typedef void    (*cleanup_op_t)(pcap_t *);

#endif

//#1. stub
static int pcap_inject_libtrace(pcap_t *handle, const void *buf, size_t size)
{
        struct pcap_libtrace *handlep = handle->priv;
        int rv = 0;

	debug("[%s() start]\n", __func__);

        return rv;
}

//#2. 
/* Set direction flag: Which packets do we accept on a forwarding
 * single device? IN, OUT or both? */
static int pcap_setdirection_libtrace(pcap_t *handle, pcap_direction_t d)
{
#ifdef HAVE_PF_PACKET_SOCKETS //XXX - where is it defined?
        struct pcap_linux *handlep = handle->priv;

        if (!handlep->sock_packet) {
                handle->direction = d;
                return 0;
        }
#endif
        /*
         * We're not using PF_PACKET sockets, so we can't determine
         * the direction of the packet.
         */
        snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
            "Setting direction is not supported on SOCK_PACKET sockets");
        return -1;
}

//#3.
static int pcap_set_datalink_libtrace(pcap_t *handle, int dlt)
{
        handle->linktype = dlt;
        return 0;
}

//#4. pcap_setnonblock_fd

//#5. pcap_getnonblock_fd

//#6. pcap_cleanup_libtrace
static void pcap_cleanup_libtrace(pcap_t *handle)
{
        debug("[%s() start]\n", __func__);

        struct pcap_libtrace *p = handle->priv;

        if (p->packet)
                trace_destroy_packet(p->packet);
        if (p->trace)
                trace_destroy(p->trace);
        free(p);

	pcap_cleanup_live_common(handle);
}

//#7. read - this should work instead of pcap_dispatch()
//pcap_dispatch() processes packets from a live capture or ``savefile'' until cnt packets are processed,  the
//end  of  the current bufferful of packets is reached when doing a live capture, the end of the ``savefile''
//is reached when reading from a ``savefile'', pcap_breakloop() is called, or an error  occurs.
static int pcap_read_libtrace(pcap_t *handle, int max_packets, pcap_handler callback, u_char *userdata)
{
        int rv;
	struct pcap_libtrace *p = handle->priv;
	struct pcap_pkthdr pcap_header;
	u_char *bp;
	libtrace_linktype_t type;
        struct timeval ts;
	long n = 1;
	int processed_packets = 0;

        debug("[%s() start]\n", __func__);

        for (n = 1; (n <= max_packets) || (max_packets < 0); n++) 
	{
		//trace_read_packet (libtrace_t *trace, libtrace_packet_t *packet)
		//will block until a packet is read (or EOF is reached).
		rv = trace_read_packet(p->trace, p->packet);
		if (rv == 0)
		{
			printf("EOF, no packets\n");
			return rv;
		}
		else if (rv < 0)
		{
			printf("error reading packet\n");
			rv = -1; return rv;	//according to man we return -1 on error
		}
		else
		{
			/* fill out pcap_header */
			gettimeofday(&ts, NULL);
			pcap_header.ts = ts;
			//Returns pointer to the start of the layer 2 header
			bp = (u_char *)trace_get_layer2(p->packet, &type, NULL);
			//uint32_t odp_packet_len(odp_packet_t pkt);
			pcap_header.len = trace_get_capture_length(p->packet);
			pcap_header.caplen = pcap_header.len;

			//callback
			callback(userdata, &pcap_header, bp);

			//increase counters
			processed_packets++;

			//check did we receive a notice from pcap_breakloop()
			if (handle->break_loop) 
			{
				handle->break_loop = 0;
				return PCAP_ERROR_BREAK;
                	}
		}
	}

	return processed_packets;
}


//#8. pcap_setfilter
static int pcap_setfilter_libtrace(pcap_t *handle, struct bpf_program *filter)
{

        return 0;
}

//#9. pcap_stats
static int pcap_stats(pcap_t *p, struct pcap_stat *ps)
{
        debug("[%s() start]\n", __func__);

        int rv = 0;

        libtrace_stat_t *stat;

        stat = trace_get_statistics(p->trace, NULL);
        if (stat)
        {
                ps->ps_recv = (unsigned int)(stat->received);
                ps->ps_drop = (unsigned int)(stat->dropped);
                ps->ps_ifdrop = (unsigned int)(stat->filtered); //filtered out
        }
        else
                rv = -1;

        return rv;
}

static int pcap_activate_libtrace(pcap_t *handle)
{
        /* Creating and initialising a packet structure to store the packets
         * that we're going to read from the trace. We store all packets here
         * alloc memory for packet and clear its fields */

	//priv is a void* ptr which points to our struct pcap_libtrace
        handle->priv->packet = trace_create_packet();
        if (!handle->priv->packet)
        {
                printf("failed to create packet (storage)\n");
                return -1;
        }

        handle->priv->trace = trace_create(source);
        if (!handle->priv->trace)
        {
                printf("failed to create trace\n");
                return -1;
        }
        else
                printf("trace created successfully\n");

	//setting functions
        handle->inject_op = pcap_inject_libtrace;
        handle->setdirection_op = pcap_setdirection_libtrace;
        handle->set_datalink_op = pcap_set_datalink_libtrace;
        handle->setnonblock_op = pcap_setnonblock_fd; /* Not our function */
        handle->getnonblock_op = pcap_getnonblock_fd; /* Not our function */
        handle->cleanup_op = pcap_cleanup_libtrace;
        handle->read_op = pcap_read_libtrace;
        handle->setfilter_op = pcap_setfilter_libtrace;
        handle->stats_op = pcap_stats_libtrace;

	//check this later

        if (handle->opt.buffer_size != 0) {
                /*
                 * Set the socket buffer size to the specified value.
                 */
                if (setsockopt(handle->fd, SOL_SOCKET, SO_RCVBUF,
                               &handle->opt.buffer_size,
                    sizeof(handle->opt.buffer_size)) == -1) {
                        snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
                                 "SO_RCVBUF: %s", pcap_strerror(errno));
                        status = PCAP_ERROR;
                        goto fail;
                }
        }

        handle->buffer = malloc(handle->bufsize + handle->offset);
        if (!handle->buffer) {
                snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
                         "malloc: %s", pcap_strerror(errno));
                status = PCAP_ERROR;
                goto fail;
        }

        handle->selectable_fd = handle->fd;

}

pcap_t* libtrace_create(const char *device, char *ebuf, int *is_ours)
{
        pcap_t *handle;
        struct pcap_libtrace *ptrace;

        *is_ours = (!strncmp(device, "trace:", 6));
        if (! *is_ours)
                return NULL;

        if (!strncmp(device, "trace:", 6)) 
	{	//we alloc auto space for pcap_t and pcap_libtrace so lets try with 0 here.
                handle = pcap_create_common((device + 6), ebuf, 0); 
                handle->selectable_fd = -1;
                ptrace = handle->priv;
                //ptrace->is_netmap = false;	//XXX - not sure we need it
        } 
	else 
	{
                handle = pcap_create_common(device, ebuf, sizeof(struct pcap_linux));
                handle->selectable_fd = -1;
                ptrace = handle->priv;
                //ptrace->is_netmap = false;	//XXX - not sure we need it
        }
        if (handle == NULL)
                return NULL;

        handle->activate_op = pcap_activate_libtrace;
        return (handle);
}


