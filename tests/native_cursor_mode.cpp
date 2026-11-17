#include "../native/cursor_mode.h"
#include <cassert>
int main(){
    osm::InputCursorMode cursor;
    cursor.mouse_moved(20,30);
    cursor.update(1,true,true,true);assert(cursor.hidden());
    cursor.mouse_moved(20,30);assert(cursor.hidden());
    cursor.mouse_moved(21,30);assert(!cursor.hidden());
    cursor.update(1,true,true,true);assert(!cursor.hidden());
    cursor.update(2,true,true,true);assert(cursor.hidden());
    cursor.update(2,true,false,true);assert(!cursor.hidden());
    cursor.update(3,true,true,false);assert(!cursor.hidden());
    cursor.update(4,true,true,true);assert(cursor.hidden());
    cursor.release();assert(!cursor.hidden());
}
