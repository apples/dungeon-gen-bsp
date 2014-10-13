#include "ranges.hpp"
#include "space.hpp"
#include "nd_rand.hpp"
#include "better_assert.hpp"

#include <algorithm>
#include <iterator>
#include <list>
#include <utility>
#include <vector>
#include <functional>
#include <random>
#include <stdexcept>
#include <iostream>

using namespace std;

//Debug printing

#if false
#define announce() (clog << __FILE__ << ":" << __LINE__ << " " << __func__ << "()" << endl)
#define show(var) (clog << "    " << #var << " = " << var << endl)
#else
#define announce() ((void)0)
#define show(var) ((void)0)
#endif

auto is_null     = [](auto* ptr){ return (ptr==nullptr); };
auto is_not_null = [](auto* ptr){ return (ptr!=nullptr); };

//Cool algorithm
template <typename Iter, typename Pred>
pair<Iter,Iter> find_bounds_if(Iter b, Iter e, Pred pred) {
    pair<Iter,Iter> rv;
    rv.first = find_if(b, e, pred);
    rv.second = find_if(reverse(e), reverse(b), pred).base();
    return rv;
}

template <typename GetLong, typename Comp>
vector<Space*> get_partition_data(vector<Space*> const& spaces, Dir dir, int lat_begin, int lat_end, GetLong get_long, Comp comp) {
    vector<Space*> rv (lat_end - lat_begin, nullptr);
    for (Space* sp : spaces) {
        assert(sp);
        auto shape = get_shape(*sp);
        for (int i = shape.begin_latitude(dir), ie = shape.end_latitude(dir); i<ie; ++i) {
            assert(i-lat_begin >= 0);
            assert(i-lat_begin < rv.size());
            auto& slot = rv[i-lat_begin];
            if (!slot || comp(get_long(shape), get_long(get_shape(*slot)))) {
                slot = sp;
            }
        }
    }
    return rv;
}

class Dungeon {
    friend class DungeonTests;

    list<Space> rooms;

    int width = 0;
    int height = 0;

    int room_width_min = 3;
    int room_height_min = 3;
    double room_ratio_min = 0.3;

    int depth_max = 5;

    mt19937 rng {nd_rand()};

    Space* room_at(int r, int c) {
        for (Space& sp : rooms) {
            auto rect = get_shape(sp);
            if (rect.begin_r <= r && rect.end_r > r && rect.begin_c <= c && rect.end_c > c) {
                return &sp;
            }
        }
        return nullptr;
    }

    pair<Space*,Space*> create_junction(Space* hall, Dir dir, int split_loc) {
        assert(hall);

        // Create new hallway
        auto newhall = add_space(Space{});
        newhall->type = SpaceType::HALL;
        newhall->data.hall = hall->data.hall;

        // Cut connections on end of hall, connect to newhall

        vector<decltype(begin(hall->neighbors))> to_erase;
        to_erase.reserve(hall->neighbors.size());

        for (auto iter : defer_range(hall->neighbors)) {
            Space* sp = *iter;
            assert(sp);
            auto rect = get_shape(*sp);
            if (rect.begin_latitude(dir) == hall->data.hall.end) {
                auto sp_iter = find(begin(sp->neighbors),end(sp->neighbors), hall);
                if (sp_iter != end(sp->neighbors)) {
                    sp->neighbors.erase(sp_iter);
                } else {
                    assert(false);
                }
                newhall->neighbors.push_back(sp);
                sp->neighbors.push_back(newhall);
                to_erase.push_back(iter);
            }
        }

        for (auto iter : reverse_range(to_erase)) {
            hall->neighbors.erase(iter);
        }

        // Physically separate hall and newhall

        hall->data.hall.end = split_loc;
        newhall->data.hall.begin = split_loc + 1;

        // Create room between hall and newhall

        auto junction = add_space(Space{});
        junction->type = SpaceType::ROOM;
        junction->data.room.begin_latitude(dir) = split_loc;
        junction->data.room.end_latitude(dir) = split_loc + 1;
        junction->data.room.begin_longitude(dir) = hall->data.hall.dir_loc;
        junction->data.room.end_longitude(dir) = hall->data.hall.dir_loc + 1;

        // Connect junction to hall and newhall

        hall->neighbors.push_back(junction);
        newhall->neighbors.push_back(junction);
        junction->neighbors.push_back(hall);
        junction->neighbors.push_back(newhall);

        pair<Space*,Space*> rv;
        rv.first = junction;
        rv.second = newhall;
        return rv;
    }

    vector<Space*> proc_collision(Space* space, Space* ptr, Dir const dir, int const loc){
        vector<Space*> rv;
        rv.reserve(2);

        switch (ptr->type) {
            case SpaceType::ROOM: {
                space->neighbors.push_back(ptr);
                ptr->neighbors.push_back(space);
            } break;

            case SpaceType::HALL: {
                auto junchall = create_junction(ptr, dir, loc);
                assert(junchall.first);
                assert(junchall.second);
                space->neighbors.push_back(junchall.first);
                junchall.first->neighbors.push_back(space);
                rv.push_back(junchall.first);
                rv.push_back(junchall.second);
            } break;

            default: {
                assert(false);
            }
        }

        return rv;
    }

    vector<Space*> carve_hallway(vector<Space*> const& first, vector<Space*> const& second, Dir dir, int lat_begin, int lat_end) {
        announce();
        show(first.size());
        show(second.size());
        show(dir);
        show(lat_begin);
        show(lat_end);

        auto first_data  = get_partition_data(first,  dir, lat_begin, lat_end, [=](const auto& v){return v.end_longitude(dir);}, greater<void>{});
        auto second_data = get_partition_data(second, dir, lat_begin, lat_end, [=](const auto& v){return v.begin_longitude(dir);},  less<void>{});

        auto first_bounds  = find_bounds_if(begin(first_data),  end(first_data),  is_not_null);
        auto second_bounds = find_bounds_if(begin(second_data), end(second_data), is_not_null);

        assert(&*first_bounds.first < &*first_bounds.second);
        assert(&*second_bounds.first < &*second_bounds.second);

        int a = max(
            first_bounds.first  - begin(first_data),
            second_bounds.first - begin(second_data));

        int b = min(
            first_bounds.second  - begin(first_data),
            second_bounds.second - begin(second_data));

        uniform_int_distribution<int> roll (a,b-1);
        int loc = roll(rng);

        assert(loc >= 0);
        assert(loc < first_data.size());
        auto  first_ptr =  first_data[loc];

        assert(loc >= 0);
        assert(loc < second_data.size());
        auto second_ptr = second_data[loc];

        assert(first_ptr && second_ptr);

        // Create hall data.
        HallData hd;
        hd.dir = dir;
        hd.dir_loc = loc + lat_begin;
        hd.begin = get_shape(*first_ptr).end_longitude(dir);
        hd.end = get_shape(*second_ptr).begin_longitude(dir);
        hd.thickness = 1;

        assert(hd.dir_loc >= lat_begin);
        assert(hd.dir_loc < lat_end);
        assert(hd.begin <= hd.end);

        // Create space object.
        auto hall = add_space(Space{});
        hall->type = SpaceType::HALL;
        hall->data.hall = hd;

        // Collide with endpoints.
        auto j1_ptrs = proc_collision(hall, first_ptr, dir, hd.dir_loc);
        auto j2_ptrs = proc_collision(hall, second_ptr, dir, hd.dir_loc);

        vector<Space*> rv;
        rv.reserve(j1_ptrs.size() + j2_ptrs.size() + 1);

        rv.push_back(hall);
        move(begin(j1_ptrs),end(j1_ptrs),back_inserter(rv));
        move(begin(j2_ptrs),end(j2_ptrs),back_inserter(rv));

        return rv;
    }

    // Small function to avoid code duplication.
    vector<Space*> try_split_recurse(Dir split_dir, Rect const rect, int const pos, int depth) {
        vector<Space*> rv;

        // Make sure the caller is sane.
        assert(
            pos > rect.begin_longitude(split_dir) &&
            pos < rect.end_longitude(split_dir));

        // Carve rooms in the two halves.
        auto recurse_rects = rect.split(split_dir, pos);

        auto first = carve_rooms(recurse_rects.first, depth+1);
        auto second = carve_rooms(recurse_rects.second, depth+1);

        //assert(room_at(recurse_rects.first.center_r(), recurse_rects.first.center_c()));
        //assert(room_at(recurse_rects.second.center_r(), recurse_rects.second.center_c()));

        // Make sure carve_rooms() succeeded.
        assert(first.size() > 0);
        assert(second.size() > 0);

        // Make sure first and second are valid.
        for (Space* sp : first) {
            assert(sp);
            auto r = get_shape(*sp);
            assert(r.begin_longitude(split_dir) >= rect.begin_longitude(split_dir));
            assert(r.end_longitude(split_dir) < rect.end_longitude(split_dir));
            assert(r.begin_latitude(split_dir) >= rect.begin_latitude(split_dir));
            assert(r.end_latitude(split_dir) < rect.end_latitude(split_dir));
        }
        for (Space* sp : second) {
            assert(sp);
            auto r = get_shape(*sp);
            assert(r.begin_longitude(split_dir) >= rect.begin_longitude(split_dir));
            assert(r.end_longitude(split_dir) < rect.end_longitude(split_dir));
            assert(r.begin_latitude(split_dir) >= rect.begin_latitude(split_dir));
            assert(r.end_latitude(split_dir) < rect.end_latitude(split_dir));
        }

        // Carve the hallway.
        auto halls = carve_hallway(first, second, split_dir, rect.begin_latitude(split_dir), rect.end_latitude(split_dir));

        // Verify that carve_hallway() succeeded/
        assert(halls.size() > 0);
        //assert(hall->data.hall.dir == split_dir);
        //assert(hall->data.hall.begin > rect.begin_longitude(split_dir));
        //assert((hall->data.hall.end) < rect.end_longitude(split_dir)); // TODO: parens needed; gcc glitch?

        // Append first and second to rv.
        rv.reserve(first.size() + second.size() + 1);
        move(
            begin(first),
            end(first),
            back_inserter(rv));
        move(
            begin(second),
            end(second),
            back_inserter(rv));

        // Add halls to rv.
        move(
            begin(halls),
            end(halls),
            back_inserter(rv));

        return rv;
    }

    struct SplitData {
        int min;
        int begin;
        int end;
        int range;
    };

    SplitData make_split_data(int min_len, int area_size, int begin, int end) const {
        SplitData rv;
        rv.min = max(
            min_len + 1, // +1 to give room for hallways.
            int(area_size * room_ratio_min));
        rv.begin = begin + rv.min;
        rv.end = end - rv.min + 1;
        rv.range = rv.end - rv.begin;
        return rv;
    }

    vector<Space*> try_split(Rect const rect, int const depth) {
        announce();
        show(rect);
        show(depth);

        vector<Space*> rv;

        int area_width = rect.end_c - rect.begin_c;
        int area_height = rect.end_r - rect.begin_r;

        const auto vsplit = make_split_data(room_height_min, area_width, rect.begin_r, rect.end_r);
        const auto hsplit = make_split_data(room_width_min, area_height, rect.begin_c, rect.end_c);

        // If there's nowhere to split, we cannot create any rooms.
        if (vsplit.range + hsplit.range <= 0) { // TODO: can be equal to 0?
            return rv;
        }

        uniform_int_distribution<int> roll (0, vsplit.range + hsplit.range - 1);

        int split = roll(rng);
        Dir split_dir = Dir::VERT;

        if (split >= vsplit.range) {
            split_dir = Dir::HORIZ;
            split = split - vsplit.range + hsplit.begin;
        } else {
            split += vsplit.begin;
        }

        rv = try_split_recurse(split_dir, rect, split, depth);

        return rv;
    }

    Rect make_room_rect(
        int min_long, int max_long,
        int min_lat, int max_lat,
        int center_long, int center_lat,
        Dir free_dir
    ) {
        uniform_int_distribution<int> roll_lat_len (
            min_lat,
            min(max_lat, int(max_long/room_ratio_min)));

        const int lat_len = roll_lat_len(rng);
        const int lat_pos = center_lat - lat_len/2;

        auto min_long_len = int(lat_len * room_ratio_min);

        uniform_int_distribution<int> roll_long_len (
            max(min_long,min_long_len),
            max_long);

        const int long_len = roll_long_len(rng);
        const int long_pos = center_long - long_len/2;

        Rect rd;
        rd.begin_longitude(free_dir) = long_pos;
        rd.end_longitude(free_dir) = long_pos + long_len;
        rd.begin_latitude(free_dir) = lat_pos;
        rd.end_latitude(free_dir) = lat_pos + lat_len;

        return rd;
    }

    Space make_room(Rect const rect) {
        announce();
        show(rect);

        const int area_width = rect.width() - 1; // -1 to give room for hallways.
        const int area_height = rect.height() - 1;

        const int center_r = rect.begin_r + area_height/2;
        const int center_c = rect.begin_c + area_width/2;

        // The free_dir is the direction in which the room can expand
        // all the way to the edge of the room.
        const Dir free_dir = (
            area_width < area_height ?
                Dir::HORIZ :
                Dir::VERT);

        Space room;
        room.type = SpaceType::ROOM;

        switch (free_dir) {
            case Dir::HORIZ: {
                room.data.room = make_room_rect(
                    room_width_min, area_width,
                    room_height_min, area_height,
                    center_c, center_r, free_dir);
            } break;

            case Dir::VERT: {
                room.data.room = make_room_rect(
                    room_height_min, area_height,
                    room_width_min, area_width,
                    center_r, center_c, free_dir);
            } break;

            default: {
                assert(false);
            }
        }

        assert(room.data.room.begin_r >= rect.begin_r);
        assert(room.data.room.end_r < rect.end_r);
        assert(room.data.room.begin_c >= rect.begin_c);
        assert(room.data.room.end_c < rect.end_c);

        return room;
    }

    vector<Space*> carve_rooms(Rect const rect, int depth) {
        announce();
        show(rect);
        show(depth);

        vector<Space*> rv;

        // Unless we're at the depth limit, try to split.
        if (depth < depth_max) {
            rv = try_split(rect, depth);
            if (!rv.empty()) {
                return rv;
            }
        }

        // We've failed to split, so just make a single room.
        rv.push_back(add_space(make_room(rect)));

        return rv;
    }

    Space* add_space(Space sp) {
        rooms.push_back(move(sp));
        return &rooms.back();
    }

public:

    template <typename T>
    void seed(T s) {
        rng.seed(s);
    }

    void go(int w, int h) {
        if (w <= room_width_min || h <= room_width_min) {
			throw logic_error("Dungeon::go(): Dungeon is too small to create any rooms!");
		}

        width = w;
        height = h;

        rooms.clear();

        carve_rooms(Rect{0, height, 0, width}, 1);
    }

    template <typename Out>
    void print_dot(Out& out) {
        out << "graph g {" << endl;
        for (Space& s : rooms) {
            Space* sp = &s;
            auto rect = get_shape(s);
            out << "    " << intptr_t(sp) << " ["
                << "label=\""
                    << (s.type==SpaceType::ROOM? "Room " : "Hall ")
                    << rect.begin_r << "-" << rect.end_r << ":"
                    << rect.begin_c << "-" << rect.end_c << "\" "
                << "pos=\""
                    << ((rect.begin_c+rect.end_c)*72/2) << ","
                    << ((rect.begin_r+rect.end_r)*72/2) << "\" "
                << "];" << endl;
            for (Space* sp2 : s.neighbors) {
                if (less<void>{}(sp, sp2)) {
                    out << "    " << intptr_t(sp) << " -- " << intptr_t(sp2) << ";" << endl;
                }
            }
        }
        out << "}" << endl;
    }

    void sub(int x) {
        width = width - x;
        height = height - x;
        for (Space& sp : rooms) {
            switch (sp.type) {
                case SpaceType::ROOM: {
                    sp.data.room.end_r -= x;
                    sp.data.room.end_c -= x;
                } break;

                case SpaceType::HALL: {
                    sp.data.hall.begin -= x;
                    sp.data.hall.thickness -= x;
                } break;

                default: {
                    throw logic_error("Invalid room detected!");
                }
            }
        }
    }

    void mult(int x) {
        width = width*x - x + 1;
        height = height*x - x + 1;
        for (Space& sp : rooms) {
            switch (sp.type) {
                case SpaceType::ROOM: {
                    sp.data.room.begin_r *= x;
                    sp.data.room.begin_c *= x;
                    sp.data.room.end_r *= x;
                    sp.data.room.end_c *= x;
                } break;

                case SpaceType::HALL: {
                    sp.data.hall.begin *= x;
                    sp.data.hall.end *= x;
                    sp.data.hall.dir_loc *= x;
                    sp.data.hall.thickness *= x;
                } break;

                default: {
                    throw logic_error("Invalid room detected!");
                }
            }
        }
    }

    int num_cols() const {
        return width;
    }

    int num_rows() const {
        return height;
    }

    list<Space> const& get_spaces() const {
        return rooms;
    }

    vector<string> print_tiles() const {
        auto line = string(num_cols(), '#');
        vector<string> tiles (num_rows(), line);
        for (auto sp : get_spaces()) {
            auto rect = get_shape(sp);
            for (int r=rect.begin_r; r<rect.end_r; ++r) {
                for (int c=rect.begin_c; c<rect.end_c; ++c) {
                    switch (sp.type) {
                        case SpaceType::ROOM: {
                            tiles[r][c] = '.';
                        } break;

                        case SpaceType::HALL: {
                            switch (sp.data.hall.dir) {
                                case Dir::HORIZ: {
                                    tiles[r][c] = '-';
                                } break;
                                case Dir::VERT: {
                                    tiles[r][c] = '|';
                                } break;
                            }
                        } break;
                    }
                }
            }
        }
        return tiles;
    }
};
