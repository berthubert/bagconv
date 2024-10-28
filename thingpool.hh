#pragma once
#include <deque>
#include <atomic>
#include <mutex>
#include <functional>
#include <string>
#include <stdexcept>

/* (C) 2024 Bert Hubert <bert@hubertnet.nl> - MIT License

Often you have an class (or Thing) that a single thread can use exclusively,
but which has some cost to create and tear down. The canonical example is
a database connection.

From ThingPool you can request a Thing, and if need be it will instantiate a
fresh instance for you. Or, in the common case, it will provide you with an
instance it made earlier.

Syntax:

ThingPool<SQLiteWriter> tp("tk.sqlite3", SQLWFlag::ReadOnly);

This says you want objects of type SQLiteWriter, and that they should be
created as: new SQLiteWriter("tk.sqlite3", SQLWFlag::ReadOnly);

Requesting an instance goes like this:

auto lease = tp.getLease();
lease->queryT("select count(1) from Document");

As long as 'lease' is alive, the instance is all yours. Once lease goes out of
scope, the object is returned to the pool. The -> syntax allows you to call
methods on SQLiteWriter. If you need a reference to your Thing, use get() (much
like smart pointers).

Many threads can use getLease() at the same time, and returns are of course
also threadsafe.

If you no longer need a lease, you can call its release() method to return it
to the pool. If you think the state of your object is bad, you can call the
abandon() method, which will delete the object and not return it to the pool.

If you allow ThreadPool to go out of scope (or if you destroy it) while there
are still active leases, this will throw an exception and likely kill your
process. Don't do this. There is likely no better robust way to deal with this
situation.

And that's it. Enjoy, feedback is welcome on bert@hubertnet.nl !
*/

template<typename T>
struct ThingPool
{
  std::deque<T*> d_pool;
  std::function<T*()> d_maker;
  std::mutex d_lock;
  std::atomic<unsigned int> d_out=0;
  std::atomic<unsigned int> d_maxout=0;

  // lifted with gratitude from https://stackoverflow.com/questions/15537817/c-how-to-store-a-parameter-pack-as-a-variable
  template<typename... Args>
  ThingPool(Args... args)
  {
    d_maker = [args...]() { return new T(args...); };
  }
  
  ~ThingPool() noexcept(false)
  {
    std::lock_guard<std::mutex> l(d_lock);

    if(d_out)
      throw std::runtime_error("Destroying ThingPool while there are still " + std::to_string(d_out) + " leases outstanding");

    for(auto& t : d_pool) {
      // std::cout<<"Deleting thing "<<(void*)t<<endl;
      delete t;
    }
  }

  void giveBack(T* thing)
  {
    std::lock_guard<std::mutex> l(d_lock);
    // cout<<"Received "<<(void*)thing<<" back for the pool"<<endl;
    --d_out;
    d_pool.push_back(thing);
  }

  void abandon(T* thing)
  {
    --d_out;
    delete thing;
  }

  void clear()
  {
    std::lock_guard<std::mutex> l(d_lock);
    for(auto& t : d_pool) {
      delete t;
    }
    d_pool.clear();
  }
  
  struct Lease
  {
    explicit Lease(ThingPool<T>* parent, T* thing)
      : d_parent(parent), d_thing(thing)
    {
      // cout<<"Instantiated a lease for "<<(void*)thing<<endl;
    }

    ~Lease()
    {
      if(d_parent) {
	// cout<<"Lease destroyed, returning thing "<<(void*)d_thing<<endl;
	d_parent->giveBack(d_thing);
      }
      else {
	// cout<<"Lease destroyed, not returning thing"<<endl;
      }
    }

    void release()
    {
      d_parent->giveBack(d_thing);
      d_thing = nullptr;
      d_parent = nullptr; 
    }

    void abandon()
    {
      d_parent->abandon(d_thing);
      d_thing = nullptr;
      d_parent = nullptr; 
    }

    Lease(const Lease&) = delete;
    
    Lease(Lease&& rhs)
    {
      d_parent = rhs.d_parent;
      d_thing = rhs.d_thing;
      rhs.d_parent = 0;
    }

    T* operator->() {
      return d_thing;
    }

    T& get()
    {
      return *d_thing;
    }
    
    ThingPool<T>* d_parent;
    T* d_thing;
  };

  Lease getLease()
  {
    std::lock_guard<std::mutex> lck(d_lock);
    if(d_pool.empty()) {
      d_pool.push_back(d_maker());
      // cout<<"Created new thing "<<(void*)d_pool.front()<<endl;
    }
    Lease l(this, d_pool.front());
    d_pool.pop_front();
    d_out++;
    if(d_out > d_maxout)
      d_maxout = (unsigned int)d_out;
    return l;
  }
};
