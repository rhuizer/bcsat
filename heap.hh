#ifndef BC_HEAP_HH
#define BC_HEAP_HH

/**
 * \brief A capacity bounded heap data structure.
 *
 * Based on the one in Sedgewick's "Algorithms in C" book.
 */

class Heap
{
  unsigned int N;
  unsigned int n;
  unsigned int *array;
  void upheap(unsigned int k);
  void downheap(unsigned int k);
public:
  /**
   * Create a new heap.
   * init() must be called after this.
   */
  Heap() {array = 0; n = 0; N = 0; }
  ~Heap();

  /**
   * Initialize the heap to have the capacity to hold \e size elements.
   */
  void init(unsigned int size);

  /**
   * Is the heap empty?
   * Time complexity is O(1).
   */
  bool empty() const {return(n==0); }

  /**
   * Remove all the elements in the heap.
   * Time complexity is O(1).
   */
  void clear() {n = 0;}

  /**
   * Insert the element \a e in the heap.
   * Time complexity is O(log(N)), where N is the number of elements
   * currently in the heap.
   */
  void insert(unsigned int e);

  /**
   * Remove and return the smallest element in the heap.
   * Time complexity is O(log(N)), where N is the number of elements
   * currently in the heap.
   */
  unsigned int remove();
};

#endif
