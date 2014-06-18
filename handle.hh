#ifndef HANDLE_HH
#define HANDLE_HH

class Handle;
class NameHandle;

#include "defs.hh"
#include "bc.hh"

class Handle
{
public:
  typedef enum {ht_UNDEFINED = 0, ht_NAME} Type;
protected:
  Gate* gate;
  Handle* next;
  Handle** prev_next_ptr;
  Type type;

public:
  Handle(Gate* const g);

  Gate* get_gate() const {return gate; }
  void change_gate(Gate* const new_gate);
  Handle* get_next() const {return next; }

public:
  Handle(const Handle& other);
  ~Handle();
  Handle& operator=(const Handle& other) {change_gate(other.gate); return *this; }
  bool is_null() const {return(!gate); }
  Type get_type() const {return type; }
};

class NameHandle : public Handle
{
protected:
  char* name;
public:
  NameHandle(Gate* const g) : Handle(g), name(0) {type = ht_NAME; }
  NameHandle(Gate* const g, char* const n) : Handle(g), name(n) {type = ht_NAME; }
  ~NameHandle() {}
  char* get_name() const {return name;}
  void set_name(char* const n) {name = n; }
};


#endif
