#include "handle.hh"


Handle::Handle(Gate* const gate) :
  gate(0), next(0), prev_next_ptr(0), type(ht_UNDEFINED)
{
  change_gate(gate);
}

Handle::Handle(const Handle& other) :
  gate(0), next(0), prev_next_ptr(0), type(ht_UNDEFINED)
{
  change_gate(other.gate);
}

Handle::~Handle()
{
  change_gate(0);
}

void Handle::change_gate(Gate * const new_gate)
{
  if(gate)
    {
      if(next)
	next->prev_next_ptr = prev_next_ptr;
      *prev_next_ptr = next;
      next = 0;
      prev_next_ptr = 0;
      gate = 0;
    }
  if(new_gate)
    {
      gate = new_gate;
      next = new_gate->handles;
      if(next)
	{
	  DEBUG_ASSERT(next->prev_next_ptr == &(new_gate->handles));
	  next->prev_next_ptr = &(this->next);
	}
      prev_next_ptr = &(new_gate->handles);
      new_gate->handles = this;
    }
}



