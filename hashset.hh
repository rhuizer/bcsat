#ifndef BC_HASHSET_HH
#define BC_HASHSET_HH

/*
 Copyright (C) Tommi Junttila
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License version 2
 as published by the Free Software Foundation.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <cstring>
#include <vector>
#include "defs.hh"

template <class Type, class KeyType, class KeyHash, class KeyComp, class KeyExtract>
class HashTable
{
private:
  class Node {
  public:
    Type value;
    unsigned int hash_value;
    Node *next;
    Node(const Type& v, const unsigned int h, Node *n) :
      value(v), hash_value(h), next(n) {}
  };
  std::vector<Node*> *buckets;
  unsigned int size;
  unsigned int size_index;
  unsigned int size_limit;
  unsigned int nof_elements;

  static const unsigned int _primes[];
  static const unsigned int _primes_nof;

  KeyHash hash_func;
  KeyComp comparator;
  KeyExtract key_extractor;

  Node* alloc_node(const Type& v, const unsigned int h, Node *n) {return new Node(v, h, n); }
  void free_node(Node *n) {delete n; }

  void grow();
public:
  HashTable();
  ~HashTable();

  class iterator
  {
    friend class HashTable;
    HashTable *hs;
    Node *node;
    iterator(HashTable *s, Node *n) {hs = s; node = n; }
  public:
    iterator(const iterator& it) {hs = it.hs; node = it.node; }
    iterator& operator=(const iterator& it) {hs = it.hs; node = it.node; return *this; }
    bool operator==(const iterator& it) {return (node == it.node); }
    bool operator!=(const iterator& it) {return (node != it.node); }
    Type& operator*() {return node->value; }
    iterator& operator++();
    iterator& operator++(int);
  };
  iterator begin();
  iterator end() {return iterator(this, 0); }

  iterator add(const Type& t);
  iterator find(const KeyType& t);
  void erase(iterator& it);
  void clear();
  bool empty() const {return(nof_elements == 0); }
};


template <class Type, class KeyType, class KeyHash, class KeyComp, class KeyExtract>
typename HashTable<Type,KeyType,KeyHash,KeyComp,KeyExtract>::iterator&
HashTable<Type,KeyType,KeyHash,KeyComp,KeyExtract>::iterator::operator++(int)
{
  return ++(*this);
}

template <class Type, class KeyType, class KeyHash, class KeyComp, class KeyExtract>
typename HashTable<Type,KeyType,KeyHash,KeyComp,KeyExtract>::iterator&
HashTable<Type,KeyType,KeyHash,KeyComp,KeyExtract>::iterator::operator++()
{
  if(node)
    {
      if(node->next)
	{
	  node = node->next;
	}
      else
	{
	  unsigned int next_bucket = (node->hash_value % hs->size) + 1;
	  while(next_bucket < hs->size && !(*(hs->buckets))[next_bucket])
	    next_bucket++;
	  if(next_bucket < hs->size)
	    node = (*(hs->buckets))[next_bucket];
	  else
	    node = 0;
	}
    }
  return *this;
}


template <class Type, class KeyType, class KeyHash, class KeyComp, class KeyExtract>
typename HashTable<Type,KeyType,KeyHash,KeyComp,KeyExtract>::iterator
HashTable<Type,KeyType,KeyHash,KeyComp,KeyExtract>::begin()
{
  Node *node = 0;
  for(unsigned int b = 0; b < size; b++)
    {
      node = (*buckets)[b];
      if(node)
	break;
    }
  return iterator(this, node);
}


template <class Type, class KeyType, class KeyHash, class KeyComp, class KeyExtract>
HashTable<Type,KeyType,KeyHash,KeyComp,KeyExtract>::HashTable()
{
  size_index = 0;
  size = _primes[size_index];
  size_limit = (unsigned int)(0.7 * (double)size);
  buckets = new std::vector<Node*>(size);
  if(!buckets) {fprintf(stderr, "Out of memory, aborting\n"); exit(1); }
  nof_elements = 0;
}


template <class Type, class KeyType, class KeyHash, class KeyComp, class KeyExtract>
HashTable<Type,KeyType,KeyHash,KeyComp,KeyExtract>::~HashTable()
{
  clear();
  if(buckets) {delete buckets; buckets = 0; }
}


template <class Type, class KeyType, class KeyHash, class KeyComp, class KeyExtract>
void
HashTable<Type,KeyType,KeyHash,KeyComp,KeyExtract>::clear()
{
  for(unsigned int b = 0; b < size; b++)
    {
      for(Node *node = (*buckets)[b]; node; )
	{
	  Node * const next = node->next;
	  free_node(node);
	  node = next;
	}
    }
  size_index = 0;
  size = _primes[size_index];
  size_limit = (unsigned int)(0.7 * (double)size);
  delete buckets;
  buckets = new std::vector<Node*>(size);
  if(!buckets) {fprintf(stderr, "Out of memory, aborting\n"); exit(1); }
  nof_elements = 0;
}


template <class Type, class KeyType, class KeyHash, class KeyComp, class KeyExtract>
void
HashTable<Type,KeyType,KeyHash,KeyComp,KeyExtract>::grow()
{
  if(size_index >= _primes_nof - 1)
    return;
  size_index++;
  const unsigned int new_size = _primes[size_index];
  std::vector<Node*> * const new_buckets = new std::vector<Node*>(new_size);
  for(unsigned int i = 0; i < size; i++)
    {
      Node *node = (*buckets)[i];
      while(node)
	{
	  Node *next_node = node->next;
	  const unsigned int new_bucket = node->hash_value % new_size;
	  node->next = (*new_buckets)[new_bucket];
	  (*new_buckets)[new_bucket] = node;
	  node = next_node;
	}
    }
  size = new_size;
  size_limit = (unsigned int)(0.7 * (double)size);
  delete buckets;
  buckets = new_buckets;
}


template <class Type, class KeyType, class KeyHash, class KeyComp, class KeyExtract>
typename HashTable<Type,KeyType,KeyHash,KeyComp,KeyExtract>::iterator
HashTable<Type,KeyType,KeyHash,KeyComp,KeyExtract>::find(const KeyType& key)
{
  const unsigned int hash_value = hash_func(key);
  const unsigned int bucket = hash_value % size;
  Node *node = (*buckets)[bucket];
  while(node)
    {
      if((node->hash_value == hash_value) && (comparator(key_extractor.get_key(node->value), key) == 0))
	{
	  return iterator(this, node);
	}
      node = node->next;
    }
  return end();
}


template <class Type, class KeyType, class KeyHash, class KeyComp, class KeyExtract>
typename HashTable<Type,KeyType,KeyHash,KeyComp,KeyExtract>::iterator
HashTable<Type,KeyType,KeyHash,KeyComp,KeyExtract>::add(const Type& value)
{
  if(nof_elements >= size_limit)
    grow();
  const unsigned int hash_value = hash_func(key_extractor.get_key(value));
  const unsigned int bucket = hash_value % size;
  Node *node = (*buckets)[bucket];
  while(node)
    {
      if((node->hash_value == hash_value) && (comparator(key_extractor.get_key(node->value), key_extractor.get_key(value)) == 0))
	{
	  return iterator(this, node);
	}
      node = node->next;
  }
  /* Not found, add */
  nof_elements++;
  Node *new_node = alloc_node(value, hash_value, (*buckets)[bucket]);
  (*buckets)[bucket] = new_node;
  return iterator(this, new_node);
}


template <class Type, class KeyType, class KeyHash, class KeyComp, class KeyExtract>
void
HashTable<Type,KeyType,KeyHash,KeyComp,KeyExtract>::erase(HashTable<Type,KeyType,KeyHash,KeyComp,KeyExtract>::iterator& it)
{
  if(!it.node)
    return;
  const unsigned int bucket = it.node->hash_value % size;
  Node *n = (*buckets)[bucket];
  if(n == it.node)
    {
      (*buckets)[bucket] = it.node->next;
      free_node(it.node);
      it.node = 0;
      nof_elements--;
      return;
    }
  while(n)
    {
      if(n->next == it.node) {
	n->next = it.node->next;
	free_node(it.node);
	it.node = 0;
	nof_elements--;
	return;
      }
      n = n->next;
    }
}


template <class Type, class KeyType, class KeyHash, class KeyComp, class KeyExtract>
const unsigned int HashTable<Type,KeyType,KeyHash,KeyComp,KeyExtract>::_primes[] = {
  5, 13, 31, 61,
  127, 251, 509, 1021,
  2039, 4091, 8191, 16381,
  32749, 66533, 131071, 262139,
  524287, 1048573, 2097143, 4194301,
  8388593, 16777213, 33554393, 67108859
};

template <class Type, class KeyType, class KeyHash, class KeyComp, class KeyExtract>
const unsigned int HashTable<Type,KeyType,KeyHash,KeyComp,KeyExtract>::_primes_nof = 24;



/** \brief A hash based set data structure.
 * This is a quick hack and only implemented because the current version of
 * the C++ standard library does not have a "hash set".
 */
template <class Type, class Hash, class Comp>
class HashSet
{
  struct _Extract {
    static const Type& get_key(const Type& v) {return v; }
  };
  HashTable<Type,Type,Hash,Comp,_Extract> ht;
public:
  HashSet() {}
  ~HashSet() {}

  typedef typename HashTable<Type,Type,Hash,Comp,_Extract>::iterator iterator;
  iterator begin() {return ht.begin(); }
  iterator end() {return ht.end(); }

  iterator add(const Type& v) {return ht.add(v); }
  iterator find(const Type& v) {return ht.find(v); }
  void erase(iterator& it) {ht.erase(it); }
  void clear() {ht.clear(); }
  bool empty() const {return ht.empty(); }
};


/** \brief A hash based map data structure.
 * This is a quick hack and only implemented because the current version of
 * the C++ standard library does not have a "hash map".
 */
template <class KeyType, class KeyHash, class KeyComp, class ValueType>
class HashMap
{
  typedef typename std::pair<KeyType, ValueType> _Pair;
  struct _Extract {
    static const KeyType& get_key(const _Pair& p) {return p.first; }
  };
  HashTable<_Pair, KeyType, KeyHash, KeyComp, _Extract> ht;
public:
  HashMap() {}
  ~HashMap() {}

  typedef typename HashTable<_Pair,KeyType,KeyHash,KeyComp,_Extract>::iterator iterator;
  iterator begin() {return ht.begin(); }
  iterator end() {return ht.end(); }

  iterator add(const KeyType& k, const ValueType& v) {return ht.add(_Pair(k, v)); }
  iterator find(const KeyType& k) {return ht.find(k); }
  void erase(iterator it) {ht.erase(it); }
  void clear() {ht.clear(); }
  bool empty() const {return ht.empty(); }
};



#endif
