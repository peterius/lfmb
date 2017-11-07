struct packet_packet
{
		char * data;
		unsigned int length;
		struct packet_packet * next;
};

extern struct packet_packet * packet_chain;