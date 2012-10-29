#include <click/pbatch.hh>
#include <g4c.h>

CLICK_DECLS

PBatch::PBatch(): capacity(CLICK_PBATCH_CAPACITY), size(0), pptrs(0), memsize(0),
		  hostmem(0), devmem(0), hpktlens(0), hslices(0), hpktannos(0),
		  dpktlens(0), dslices(0), dpktannos(0), slice_begin(0),
		  slice_end(0), slice_length(0), slice_size(0),
		  anno_flags(0), anno_size(0), dev_stream(0),
		  hwork_ptr(0), dwork_ptr(0), work_size(0), work_data(0)
{
}

PBatch::PBatch(int _capacity, int _slice_begin, int _slice_end,
	       int _anno_flags, int _anno_length):
	capacity(_capacity), size(0), pptrs(0),
	hostmem(0), devmem(0), hpktlens(0), hslices(0), hpktannos(0),
	dpktlens(0), dslices(0), dpktannos(0),
	anno_flags(_anno_flags), dev_stream(0),
	hwork_ptr(0), dwork_ptr(0), work_size(0), work_data(0)
{
	calculate_parameters();
}


PBatch::~PBatch() {}

void
PBatch::calculate_parameters()
{
	slice_begin = _slice_begin;
	if (_slice_end < 0) {
		slice_end = -1;
		slice_length = CLICK_PBATH_PACKET_BUFFER_SIZE;
		slice_size = slice_length;
	} else {
		slice_end = _slice_end;
		slice_length = _slice_end - _slice_begin;
		slice_size = g4c_round_up(slice_length, 32);
	}

	if (_anno_flags == 0)
		anno_size = 0;
	else
		anno_size = g4c_round_up(_anno_length, 32);

	memsize = 0;
	if (_slice_end == -1)
		memsize += g4c_round_up(sizeof(short)*capacity, G4C_PAGE_SIZE);

	memsize += g4c_round_up(slice_size*capacity, G4C_PAGE_SIZE);

        if (_anno_flags != 0)
		memsize += g4c_round_up(anno_size*capacity, G4C_PAGE_SIZE);
}

int
PBatch::init_for_host_batching()
{
	if (memsize == 0) // unstable sign
		calculate_parameters();
	
	pptrs = new Packet*[capacity];
	if (!pptrs)
		return -1;
	
	memset(pptrs, 0, sizeof(Packet*)*capacity);
	return 0;
}

void
PBatch::clean_for_host_batching()
{
	delete[] pptrs;
}

CLICK_ENDDECLS