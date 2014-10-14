#include "dungeon.hpp"

#include <iostream>
using namespace std;

#define TEST(B) (((B)&&(clog<<"PASS"<<endl,true))||(clog<<"FAIL: "<<__FILE__<<":"<<__LINE__<<endl,false))
struct DungeonTests {
    Dungeon dung;

    static constexpr auto br = 12;
    static constexpr auto er = 42;
    static constexpr auto bc = 13;
    static constexpr auto ec = 27;

    bool test_rect_axes() {
        Rect rect;
        rect.begin_r = br;
        rect.end_r = er;
        rect.begin_c = bc;
        rect.end_c = ec;

        bool rv = true;

        rv*=TEST(( addressof(rect.begin_r) == addressof(rect.begin_longitude(Dir::VERT))  ));
        rv*=TEST(( addressof(rect.end_r)   == addressof(rect.end_longitude(Dir::VERT))    ));
        rv*=TEST(( addressof(rect.begin_c) == addressof(rect.begin_latitude(Dir::VERT))   ));
        rv*=TEST(( addressof(rect.end_c)   == addressof(rect.end_latitude(Dir::VERT))     ));
        rv*=TEST(( addressof(rect.begin_c) == addressof(rect.begin_longitude(Dir::HORIZ)) ));
        rv*=TEST(( addressof(rect.end_c)   == addressof(rect.end_longitude(Dir::HORIZ))   ));
        rv*=TEST(( addressof(rect.begin_r) == addressof(rect.begin_latitude(Dir::HORIZ))  ));
        rv*=TEST(( addressof(rect.end_r)   == addressof(rect.end_latitude(Dir::HORIZ))    ));

        rv*=TEST(( rect.width() == (ec-bc) ));
        rv*=TEST(( rect.height() == (er-br) ));

        return rv;
    }

    bool test_room_shape() {
        Rect rect {br,er,bc,ec};

        Space space;
        space.type = SpaceType::ROOM;
        space.data.room = rect;

        Rect shape = get_shape(space);

        bool rv = true;
        rv*=TEST(( shape.begin_r == rect.begin_r ));
        rv*=TEST(( shape.end_r == rect.end_r ));
        rv*=TEST(( shape.begin_c == rect.begin_c ));
        rv*=TEST(( shape.end_c == rect.end_c ));
        return rv;
    }

    bool test_vert_hall_shape() {
        HallData dat;
        dat.dir = Dir::VERT;
        dat.dir_loc = bc;
        dat.begin = br;
        dat.end = er;
        dat.thickness = 1;

        Space space;
        space.type = SpaceType::HALL;
        space.data.hall = dat;

        Rect shape = get_shape(space);

        bool rv = true;
        rv*=TEST(( shape.begin_r == br ));
        rv*=TEST(( shape.end_r == er   ));
        rv*=TEST(( shape.begin_c == bc ));
        rv*=TEST(( shape.end_c == bc+1 ));
        return rv;
    }

    bool test_horiz_hall_shape() {
        HallData dat;
        dat.dir = Dir::HORIZ;
        dat.dir_loc = br;
        dat.begin = bc;
        dat.end = ec;
        dat.thickness = 1;

        Space space;
        space.type = SpaceType::HALL;
        space.data.hall = dat;

        Rect shape = get_shape(space);

        bool rv = true;
        rv*=TEST(( shape.begin_r == br ));
        rv*=TEST(( shape.end_r == br+1 ));
        rv*=TEST(( shape.begin_c == bc ));
        rv*=TEST(( shape.end_c == ec   ));
        return rv;
    }

    bool run_all_tests() {
        bool rv = true;
        rv *= test_rect_axes();
        rv *= test_room_shape();
        rv *= test_vert_hall_shape();
        rv *= test_horiz_hall_shape();
        return rv;
    }
};
#undef TEST

void printit(Dungeon& dung) {
    auto tiles = dung.print_tiles();

    for (auto& l : tiles) {
        cout << l << "\n";
    }
    cout << endl;
}

#include <sstream>
#include <cstdlib>
#include <chrono>
using namespace std;
using namespace std::chrono;

Dungeon dung;

template <typename F>
auto bench(F&& f) {
    auto start = high_resolution_clock::now();
    f();
    auto end = high_resolution_clock::now();
    return (end-start);
}

int main(int argc, char* argv[]) try {
    DungeonTests tests;
	if (tests.run_all_tests()) {
        if (argc < 3) {
            cout << "NEED W AND H" << endl;
            return -1;
        }
        auto seed = nd_rand();
        if (argc == 5) {
            stringstream(string(argv[4])) >> seed;
        }
        cout << endl << "Seed: " << seed << endl << endl;
        duration<double,milli> mint {};
        duration<double,milli> maxt {};
        duration<double,milli> avg {};
        constexpr auto loops = 1000;
        for (int i=0; i<loops; ++i) {
            auto t = bench([&]{dung.go(atoi(argv[1]),atoi(argv[2]));});
            if (i==0 || t<mint) {
                mint = t;
            }
            if (i==0 || t>maxt) {
                maxt = t;
            }
            avg += t;
        }
        avg /= loops;
        cout << "mint= " << mint.count() << endl;
        cout << "avg=  " << avg.count() << endl;
        cout << "maxt= " << maxt.count() << endl;
        dung.seed(seed);
        dung.go(atoi(argv[1]),atoi(argv[2]));
        printit(dung);
        if (argc >= 4) {
            void (Dungeon::*func)(int) = &Dungeon::mult;
            for (char c : string(argv[3])) {
                if (c == '-') {
                    func = &Dungeon::sub;
                } else {
                    (dung.*func)(c-'0');
                    func = &Dungeon::mult;
                }
            }
        }
        printit(dung);
	}
} catch (exception const& e) {
    cerr << "EXCEPTION!" << endl;
    cerr << e.what() << endl;
    printit(dung);
    dung.print_dot(cerr);
    return -1;
}
