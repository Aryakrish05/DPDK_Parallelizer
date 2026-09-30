# DSL constraints

- Loopless
- No function calls other than the primitives exposed in the language
- Only one function `int process(struct phv pkt)`
- Every path must end in `return DROP|FORWARD;` **(Anything else needed?)**
- All data structures shall be language primitives which are defined like maps, for now we have only maps and nothing else
- We don't worry about byte-orders, code generation takes care of this using `ntohs()`
- Has `Extract(phv.hdrname)` for extractions, **we assume lookaheads are not there and thus our dsl cannot handle ip options and the like**. After every extract, the header becomes accessible. The packet will be dropped implicitly if the packet length check was not satisfied. Each extract will translate to idiomatic bpf code


# Types and operators
- Primitives - `bool`,`uint8_t`, `uint16_t`,... (so that we can infer the sizes if needed). It may be better to avoid ints,chars and ... which may not really add more information to our analyses
- Normal structs as in C
- **Do we want to disallow some C operators?**

# Constants
- if i am using pycparser, allow as many things as possible otherwise just stick to `const <type> <var> = <value>`
- Anything is fine as long as grammar doesn't become too complex

# Maps
- **For now, this is the only non-C thing**
- Would like to have some metadata in each map definition to help the analysis

```
map <name> {
    key_type = ...;
    value_type = ...;
    size = ...; 
    lock_type = ...; 

    //error codes or representations
    key_not_found = <thing of value_type>;
    insertion_failure_map_full = <some int>;
    deletion_failure_key_not_found = <some int>;

    implementation_header_file = ...;
}
```
- we have the size as some fixed value as I think dynamic allocation is rarely done in dpdk code
- lock_type is one of:
    - **key:** one lock for each key
    - **global:** the entire map is locked
    - **lockfree:** this is for when the order doesn't matter at all, includes the commutative and associative operations like a counter
    - **readonly:** this is for maps which are read-only
- We ask the user to give his implementation for the map with the locking mechanism

- The following functions are expected to be implemented correctly by the user:
    - `map.get(key)`- returns a copy which can be modified and written back with a write **(Should we instead return pointers or something - this feels the easiest)**. It must be enforced that if get fails, the value returned is not used for further processing
    - `map.put(key,value)` - **Need to decide what people do on insertion failure**
    - `map.erase(key)` - **Need to decide this as well**
    - `map.lock()` and `map.unlock()` - this is for map types which are globally locked
    - `map.lock(key)` and `map.unlock(key)` - for key-level locking
    - **Still thinking what to expose for lock-free**
- We can check that the above things are implemented either by using type_traits or concepts in C++



# Primitive techniques

## Slicing

- PENDING

## Symbolic Execution

- We shall use z3 for this, maintain a dictionary of variable names to their values(which will just be functions of packet fields), we shall also maintain information on packet fields.
- We are guaranteed to do this for no state dependence of access, so we can just have 
- Walk along the cfg recursively, add constraints when you take a branch, update the values of variable names with appropriate expressions at every definition and perform whatever you need to at the state access position
- Planning to have multiple free variables for packet fields, instead of one symbolic packet to make the representations easier and thereby avoiding Extracts and bitblasting entirely
- **Extracts shall be ignored by the symbolic executor - is this an issue???**

## State dependence of access

- We would like to find out if any access (map or the key) depends on the **value** of some state which may be modified (**so we ignore read-only maps**)
- **Note that an access just refers to the map and the key and doesn't care about the value**
- For every map access, we can identify what influences their values, both control and data dependencies, i.e from the backward slice. If any backward slice contains a modifiable map reading **(assuming that the user writes sensible code and nothing like `x=map.get(k1)-map.get(k1)`)**, then there is state dependence of the key.

- Two things are clear if this happens
    
    1.  **For locking:** It is not possible to figure out which locks to obtain before reading any state. This means that we can't do snapshot locking where we acquire appropriate locks for all the keys we access for a packet
    2. **For sharding:**  If at all we are going to shard, we would have to put the map whose access depends on previous state all into one core and all packets which satisfy the conditions resulting in such accesses will be needed to be forwarded to that core. For now, **we will not employ sharding in these cases** similar to what Maestro is doing.


# Sharing strategies

- Strategies for locking which assume the strict condition that the resulting state and outcomes of the parallel processing is as if packets were processed in some sequential order by one core and an NF described by the DSL
- The techniques are considered in the most general sense and optimisations shall be done later
- **Should we ask the user to mention what kind of locking he wants?? - Feels like a good idea** 

## 1. Snapshot locking

- If the set of maps and keys is a pure function of a packet, we can avoid deadlocks by acquiring locks in a particular pre-defined order 
- **Algorithm** 
    - Run the state dependence of access check
    - If state dependence is absent, we would like to identify the union of the backward slices of each of the accesses (only the keys and maps). 
    - We maintain an array and for each access we add the `(map,key)` pair to the array, which we shall sort(first in the increasing order of the map address and then in increasing order of the keys). **Note that the user decides the map implementation and may choose to do fine grained or coarse grained locking. If the map is globally locked, just add to the array only once**
    - For each element in the list, first we acquire locks and then release anytime a return is executed.**Optimisation is to release after the last use of the map+key pair or map, for later versions**
    - There will also be a "sequential" copy of the original code printed which takes care of the actual computation.

## 2. 2PL with deadlock prevention
- When we can't do snapshot locking, we can always do 2PL with deadlock prevention
- **Algorithm**
    - We have the regular code and locks are acquired when needed and we maintain which locks we have acquired. For globally locked maps, we wish to not acquire twice.
    - Again care must be taken to release locks at the end
    - To avoid deadlocks, we will maintain a timestamp for each core. This shall be done using say an atomic timestamp counter. Whenever we have a packet arriving(i.e at the beginning of the process function), we do an atomic increment and use the previous value. This will be the timestamp of the packet and hence the core for now - the timestamp for each core shall be maintained in an array, which is readable by all cores.
    - While acquiring a lock, we can have two different techniques
        - **Wound-wait:** a core with an earlier timestamp acquires the lock if it is available and steals the lock from the one with the later timestamp if it holding it. If it steals the lock, the core gets wounded and has to abort its computations and retry
        - **Wait-die:** a core with an earlier timestamp waits for the core with a later timestamp to finish if it wants the lock held by it, but if a core with a later timestamp wants to get a lock of something with an earlier timestamp, it aborts and retries.
        - Since they are timestamp based, there is not going to be starvation as well.
        - These would need the locks to be modified accordingly. The locks would need to maintain a reference to the array of timestamps and have a mechanism of taking care of them.
    - Both of these techniques need to have a retry loop after the timestamp acquisition within which the entire program sits. 
    - To ensure that aborts are possible without corrupting state, we can have two different copies of the state, and have a bit which tells which copy should be accessed (must be at as small a granularity as possible to prevent copying). When you write, you always write to the inactive copy and at the end, when the program has terminated successfully - i.e, you release locks while returning, you end up swapping the bit appropriately for the states you wrote to. So, in some sense, you will need to keep track of which states you wrote to as well.

- **Lock Designs**
    - To be decided, not a priority for the DSL/Compiler Design

## 3. Read-only states
- These can be shared purely without locks. 
- Identify all maps which don't have a `map.put(...,...)`, they may just be shared like this
- In the first pass, we would like to forget such maps as a whole to prevent ending up locking them

## 4. Commutative and Associative operations whose reads don't affect other states or output
- **How do I identify such operations**
- Order of operations don't matter and thus, we may just use atomics or do fine grained locking.
- Expecting stuff like drop count and statistics to be of this format


# SCR strategies

- **Do we need NO state dependence of access??**
- Need to identify the minimum set of packet fields which are used in updating state
- **Algorithm for metadata identification:**
    - For each state updation or read(value, key and map), find the backward slice for the whole statement and identify all the packet dependencies for any statement in the slice
    - The union of all the packet dependencies should mention the metadata needed
## Catching up state
- **We would need to fix a format for packet:** I am okay with the dummy ethernet header and metadata elements stacked up after that before the actual headers. **But how to get this done in an optimised way??**
- Then it's just a trivial loop for `NUM_CORES-1` iterations
- Again here you will need the entire backward slice and you shall just remove the extracts and replace `phv.hdr.elem` with `meta.hdr_elem`
- Then we can do dead-code elimination

## Packet History Sequencer
- Will first sit in our PacketDistributor
- **Depends on the packet design.Plan similar to Tofino primitives so that some experience is there to generate such code**

# Sharding strategies
- **We just say that sharding will not be done if there is state dependence of access**
- If there is no state dependence of access, we can have for each state access, an expression for the key in terms of a symbolic packet and conditions which should be true for that access to happen. We shall collect a list of such pairs of `cond(pkt)` and `key(pkt)` expressions for every map.
- We shall use the function `f : packet -> core_no`  
- If we want two packets `p1` and `p2` such that `f(p1)!=f(p2)`, then for all pairs of state accesses for every map, we must **NOT** have `cond(p1) and cond(p2) and key(p1)=key(p2)` as that would mean that the two packets access the same state although they are on different cores.
- This gives us a bunch of constraints on `f` and we may synthesise the hash function or the packet distributor using synthesis
- This is a more general analysis than that done in the maestro paper. For RSS, we would have to do something similar to what is done in the paper or constrain our variables for synthesis accordingly.
- **What is the broad format for our distributor??**
- **Look into the examples mentioned in the Maestro Paper, to get a better idea - maybe summarize here**