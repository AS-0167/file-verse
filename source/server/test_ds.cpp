#include <iostream>
#include "../data_structures/dynamic_array.hpp"
#include "../data_structures/linked_list.hpp"
#include "../data_structures/bitmap.hpp"
#include "../data_structures/hashmap.hpp"
#include "../data_structures/fifo_queue.h"

int main() {
    DynamicArray<int> da; da.push_back(10); da.push_back(20);
    std::cout << "DA size=" << da.getSize() << " first=" << da[0] << "\n";


    LinkedList<std::string> ll; ll.push_back("a"); ll.push_back("b");
    std::cout << "LL size=" << ll.size() << " front=" << *ll.begin() << "\n";


    Bitmap bm; bm.reset(16); bm.set(3);
    std::cout << "BM test(3)=" << bm.test(3) << " free_run2=" << bm.find_free_run(2) << "\n";


    HashMap<std::string,int> hm; hm.put("x", 42);
    std::cout << "HM x=" << (hm.get("x") ? *hm.get("x") : -1) << "\n";

    FIFOQueue<int> q;
    q.enqueue(7);
    q.enqueue(8);
    std::cout << "Q pop=" << q.dequeue() << " size=" << q.size() << "\n";

    return 0;
}
