#include "ranges.hpp"
#include "space.hpp"
#include "nd_rand.hpp"
#include "better_assert.hpp"
#include "array_vector.hpp"
#include "array_view.hpp"
#include "thread_worker.hpp"

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>
#include <list>
#include <functional>
#include <random>
#include <stdexcept>
#include <iostream>
#include <future>
#include <mutex>

using namespace std;

auto is_null     = [](auto* ptr){ return (ptr==nullptr); };
auto is_not_null = [](auto* ptr){ return (ptr!=nullptr); };

//Cool algorithm
template <typename Iter, typename Pred>
pair<Iter,Iter> find_partition_bounds(Iter b, Iter e, Pred pred) {
    pair<Iter,Iter> rv;
    rv.first = partition_point(b, e, pred);
    rv.second = partition_point(reverse(e), reverse(b), pred).base();
    return rv;
}

enum class Cardinal {
    NORTH,
    SOUTH,
    WEST,
    EAST,
};

constexpr auto cardinals = {
    Cardinal::NORTH,
    Cardinal::SOUTH,
    Cardinal::WEST,
    Cardinal::EAST,
};

inline ostream& operator<<(ostream& out, Cardinal const& car) {
    static const string strs[] = {
        "NORTH",
        "SOUTH",
        "WEST",
        "EAST",
    };
    out << strs[int(car)];
    return out;
}

struct CardinalView {
    pair<Cardinal,Cardinal> longitude;
    pair<Cardinal,Cardinal> latitude;
};

inline CardinalView get_cardinals(Dir dir) {
    switch (dir) {
        case Dir::HORIZ: {
            return CardinalView{
                make_pair(Cardinal::WEST,Cardinal::EAST),
                make_pair(Cardinal::NORTH,Cardinal::SOUTH)};
        } break;
        case Dir::VERT: {
            return CardinalView{
                make_pair(Cardinal::NORTH,Cardinal::SOUTH),
                make_pair(Cardinal::WEST,Cardinal::EAST)};
        } break;
        default: {
            throw logic_error("Unknown direction!");
        } break;
    }
}

inline Dir car2dir(Cardinal car) {
    constexpr Dir dirs[] = {Dir::VERT,Dir::VERT,Dir::HORIZ,Dir::HORIZ};
    return dirs[int(car)];
}

template <typename Comp>
vector<Space*> get_partition_data(ArrayView<Space>& spaces, Dir dir, int lat_begin, int lat_end, Comp comp) {
    vector<Space*> rv (lat_end - lat_begin, nullptr);
    for (Space& sp : spaces) {
        auto shape = get_shape(sp);
        for (int i = shape.begin_latitude(dir), ie = shape.end_latitude(dir); i<ie; ++i) {
            assert(i-lat_begin >= 0);
            assert(i-lat_begin < rv.size());
            auto& slot = rv[i-lat_begin];
            if (!slot || comp(shape, get_shape(*slot))) {
                slot = &sp;
            }
        }
    }
    return rv;
}

struct AreaData {
    Rect rect;
    ArrayView<Space*> views[4];
    ArrayView<Space> spaces;

    ArrayView<Space*>& get_view(Cardinal car) {
        AreaData const& self = *this;
        return const_cast<ArrayView<Space*>&>(self.get_view(car));
    }

    ArrayView<Space*> const& get_view(Cardinal car) const {
        return views[int(car)];
    }

    Space*& view_from(Cardinal car, int loc) {
        auto pos = loc-rect.begin_latitude(car2dir(car));
        assert(pos >= 0);
        assert(pos < get_view(car).size());
        return get_view(car)[pos];
    }

    void bind_to(ArrayView<Space*> const& cache) {
        assert(!cache.empty());
        auto arr = cache.slice(0);
        auto mb = arr.begin();
        get_view(Cardinal::NORTH) = ArrayView<Space*>(mb,mb+rect.width());
        mb += rect.width();
        get_view(Cardinal::SOUTH) = ArrayView<Space*>(mb,mb+rect.width());
        mb += rect.width();
        get_view(Cardinal::WEST) = ArrayView<Space*>(mb,mb+rect.height());
        mb += rect.height();
        get_view(Cardinal::EAST) = ArrayView<Space*>(mb,mb+rect.height());
    }

    bool verify() {
        return (
            get_view(Cardinal::NORTH).size() == get_view(Cardinal::SOUTH).size() &&
            get_view(Cardinal::WEST).size() == get_view(Cardinal::EAST).size());
    }
};

class Dungeon {
    friend class DungeonTests;

    using SpaceVec = vector<Space>;
    SpaceVec rooms;

    int width = 0;
    int height = 0;

    int room_width_min = 3;
    int room_height_min = 3;
    double room_ratio_min = 0.3;

    int depth_max = 15;

    mt19937 rng {nd_rand()};
    uniform_int_distribution<int> dist;

    vector<Space*> cache;
    size_t cache_pos = 0;

    struct CacheViewHandle {
        Dungeon* dung;
        ArrayView<Space*> view;

        CacheViewHandle(Dungeon* dung, size_t sz) : dung(dung) {
            view = ArrayView<Space*>(&dung->cache[dung->cache_pos], sz);
            dung->cache_pos += sz;
        }

        ~CacheViewHandle() {
            assert(&dung->cache[dung->cache_pos] == view.end());
            dung->cache_pos -= view.size();
        }
    };

    CacheViewHandle get_cache_view(size_t sz) {
        if (cache_pos + sz > cache.size()) {
            throw runtime_error("Out of cache memory!");
        }
        auto rv = CacheViewHandle(this, sz);
        fill(begin(rv.view),end(rv.view),nullptr);
        return rv;
    }

    int roll_rng(int a, int b) {
        auto params = uniform_int_distribution<int>::param_type(a,b);
        return dist(rng,params);
    }

    ArrayView<Space> create_junction(Space* hall, Dir dir, int split_loc) {
        assert(hall);

        auto junction = add_space(Space{});
        auto newhall = add_space(Space{});
        auto rv = ArrayView<Space>(newhall,newhall+2);

        // Create new hallway
        newhall->type = SpaceType::HALL;
        newhall->data.hall = hall->data.hall;

        // Cut connections on end of hall, connect to newhall

        auto iter = find_if(begin(hall->neighbors),end(hall->neighbors), [=](Space* sp){
            auto rect = get_shape(*sp);
            return (rect.begin_latitude(dir) == hall->data.hall.end);
        });

        assert(sp);

        Space* sp = *iter;

        auto sp_iter = find(begin(sp->neighbors),end(sp->neighbors), hall);
        assert(sp_iter != end(sp->neighbors));

        sp->neighbors.erase(sp_iter);
        sp->neighbors.push_back(newhall);

        newhall->neighbors.push_back(sp);
        hall->neighbors.erase(iter);

        // Physically separate hall and newhall

        hall->data.hall.end = split_loc;
        newhall->data.hall.begin = split_loc + 1;

        // Create room between hall and newhall

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

        return rv;
    }

    //using ProcCollideResults = vector<Space*>;
    using ProcCollideResults = ArrayVector<Space*,2>;

    ArrayView<Space> proc_collision(Space* space, Space* ptr, Dir const dir, int const loc) {
        ArrayView<Space> rv;

        switch (ptr->type) {
            case SpaceType::ROOM: {
                space->neighbors.push_back(ptr);
                ptr->neighbors.push_back(space);
            } break;

            case SpaceType::HALL: {
                rv = create_junction(ptr, dir, loc);
                space->neighbors.push_back(&rv[0]);
                rv[0].neighbors.push_back(space);
            } break;

            default: {
                assert(false);
            }
        }

        return rv;
    }

    ArrayView<Space> carve_hallway(AreaData const& first, AreaData const& second, Dir dir) {
        auto cards = get_cardinals(dir);

        auto& first_data = first.get_view(cards.longitude.second);
        auto& second_data = second.get_view(cards.longitude.first);

        assert(first_data.size() == second_data.size());

        auto first_bounds  = find_partition_bounds(begin(first_data),  end(first_data),  is_null);
        auto second_bounds = find_partition_bounds(begin(second_data), end(second_data), is_null);

        assert(&*first_bounds.first < &*first_bounds.second);
        assert(&*second_bounds.first < &*second_bounds.second);

        int a = max(
            first_bounds.first  - begin(first_data),
            second_bounds.first - begin(second_data));

        int b = min(
            first_bounds.second  - begin(first_data),
            second_bounds.second - begin(second_data));

        assert(b-a > 0);

        int loc = roll_rng(a,b-1);

        assert(loc >= 0);
        assert(loc < first_data.size());
        auto first_ptr = first_data[loc];

        assert(loc >= 0);
        assert(loc < second_data.size());
        auto second_ptr = second_data[loc];

        assert(first_ptr && second_ptr);

        // Create hall data.
        HallData hd;
        hd.dir = dir;
        hd.dir_loc = loc + first.rect.begin_latitude(dir);
        hd.begin = get_shape(*first_ptr).end_longitude(dir);
        hd.end = get_shape(*second_ptr).begin_longitude(dir);
        hd.thickness = 1;

        assert(hd.dir_loc >= first.rect.begin_latitude(dir));
        assert(hd.dir_loc < first.rect.end_latitude(dir));
        assert(hd.begin <= hd.end);

        // Create space object.
        auto hall = add_space(Space{});
        hall->type = SpaceType::HALL;
        hall->data.hall = hd;

        // Collide with endpoints.
        auto junc1 = proc_collision(hall, first_ptr, dir, hd.dir_loc);
        auto junc2 = proc_collision(hall, second_ptr, dir, hd.dir_loc);

        return ArrayView<Space>(hall, hall + junc1.size() + junc2.size() + 1);
    }

    // Small function to avoid code duplication.
    AreaData try_split_recurse(Dir split_dir, AreaData area, int const pos, int depth) {
        // Make sure the caller is sane.

        assert(area.verify());

        assert(!area.get_view(Cardinal::NORTH).empty());
        assert(!area.get_view(Cardinal::SOUTH).empty());
        assert(!area.get_view(Cardinal::WEST).empty());
        assert(!area.get_view(Cardinal::EAST).empty());

        assert(area.get_view(Cardinal::NORTH).size() == area.rect.width());
        assert(area.get_view(Cardinal::SOUTH).size() == area.rect.width());
        assert(area.get_view(Cardinal::WEST).size() == area.rect.height());
        assert(area.get_view(Cardinal::EAST).size() == area.rect.height());

        auto cards = get_cardinals(split_dir);

        assert_if (split_dir == Dir::HORIZ) {
            assert(cards.longitude.first == Cardinal::WEST);
            assert(cards.longitude.second == Cardinal::EAST);
            assert(cards.latitude.first == Cardinal::NORTH);
            assert(cards.latitude.second == Cardinal::SOUTH);
        }
        assert_if (split_dir == Dir::VERT) {
            assert(cards.latitude.first == Cardinal::WEST);
            assert(cards.latitude.second == Cardinal::EAST);
            assert(cards.longitude.first == Cardinal::NORTH);
            assert(cards.longitude.second == Cardinal::SOUTH);
        }

        assert(
            pos > area.rect.begin_longitude(split_dir) &&
            pos < area.rect.end_longitude(split_dir));

        assert(area.rect.width() >= room_width_min);
        assert(area.rect.height() >= room_height_min);

        // Carve rooms in the two halves.
        auto recurse_rects = area.rect.split(split_dir, pos);

        assert(
            recurse_rects.first.len_longitude(split_dir) +
            recurse_rects.second.len_longitude(split_dir) ==
            area.rect.len_longitude(split_dir));
        assert(recurse_rects.first.len_latitude(split_dir) == area.rect.len_latitude(split_dir));
        assert(recurse_rects.second.len_latitude(split_dir) == area.rect.len_latitude(split_dir));

        assert(area.get_view(cards.longitude.first).size() == area.rect.len_latitude(split_dir));
        assert(area.get_view(cards.longitude.second).size() == area.rect.len_latitude(split_dir));

        auto first_cache = get_cache_view(area.rect.len_latitude(split_dir));

        AreaData first_in;
        first_in.rect = recurse_rects.first;
        first_in.get_view(cards.longitude.first) = area.get_view(cards.longitude.first);
        first_in.get_view(cards.longitude.second) = first_cache.view;
        first_in.get_view(cards.latitude.first) = area.get_view(cards.latitude.first).slice(0,recurse_rects.first.len_longitude(split_dir));
        first_in.get_view(cards.latitude.second) = area.get_view(cards.latitude.second).slice(0,recurse_rects.first.len_longitude(split_dir));
        assert(first_in.verify());

        assert(first_in.get_view(Cardinal::NORTH).size() == first_in.rect.width());
        assert(first_in.get_view(Cardinal::SOUTH).size() == first_in.rect.width());
        assert(first_in.get_view(Cardinal::WEST).size() == first_in.rect.height());
        assert(first_in.get_view(Cardinal::EAST).size() == first_in.rect.height());

        auto first = carve_rooms(first_in, depth+1);

        auto second_cache = get_cache_view(area.rect.len_latitude(split_dir));
        AreaData second_in;
        second_in.rect = recurse_rects.second;
        second_in.get_view(cards.longitude.first) = second_cache.view;
        second_in.get_view(cards.longitude.second) = area.get_view(cards.longitude.second);
        second_in.get_view(cards.latitude.first) = area.get_view(cards.latitude.first).slice(recurse_rects.first.len_longitude(split_dir));
        second_in.get_view(cards.latitude.second) = area.get_view(cards.latitude.second).slice(recurse_rects.first.len_longitude(split_dir));
        assert(second_in.verify());

        assert(second_in.get_view(Cardinal::NORTH).size() == second_in.rect.width());
        assert(second_in.get_view(Cardinal::SOUTH).size() == second_in.rect.width());
        assert(second_in.get_view(Cardinal::WEST).size() == second_in.rect.height());
        assert(second_in.get_view(Cardinal::EAST).size() == second_in.rect.height());

        auto second = carve_rooms(second_in, depth+1);

        // Make sure carve_rooms() succeeded.
        assert(first.spaces.size() > 0);
        assert(second.spaces.size() > 0);
        assert(first.verify());
        assert(second.verify());

        // Make sure first and second are valid.

        // Carve the hallway.
        auto halls = carve_hallway(first, second, split_dir);

        // Verify that carve_hallway() succeeded/
        assert(halls.size() > 0);

        // Append first and second to rv.

        assert(area.get_view(cards.longitude.first) == first.get_view(cards.longitude.first));
        assert(area.get_view(cards.longitude.second) == second.get_view(cards.longitude.second));

        for (int i : number_range(0,int(area.get_view(cards.longitude.first).size()))) {
            if (!area.get_view(cards.longitude.first)[i]) {
                area.get_view(cards.longitude.first)[i] = second.get_view(cards.longitude.first)[i];
            }
            if (!area.get_view(cards.longitude.second)[i]) {
                area.get_view(cards.longitude.second)[i] = first.get_view(cards.longitude.second)[i];
            }
        }

        auto comp_lat_begin = [&](auto const& a, auto const& b){
            return a.begin_latitude(split_dir) <= b.begin_latitude(split_dir);
        };

        auto comp_lat_end = [&](auto const& a, auto const& b){
            return a.end_latitude(split_dir) >= b.end_latitude(split_dir);
        };

        auto comp_long_begin = [&](auto const& a, auto const& b){
            return a.begin_longitude(split_dir) <= b.begin_longitude(split_dir);
        };

        auto comp_long_end = [&](auto const& a, auto const& b){
            return a.end_longitude(split_dir) >= b.end_longitude(split_dir);
        };

        for (Space& sp : halls) {
            auto sp_rect = get_shape(sp);
            auto check_assign = [&](int i, auto car, auto const& comp){
                Space*& cur = area.view_from(car,i);
                if (!cur || comp(sp_rect,get_shape(*cur))) {
                    cur = &sp;
                }
                assert(cur);
            };
            for (int i : sp_rect.range_longitude(split_dir)) {
                check_assign(i,cards.latitude.first,comp_lat_begin);
                check_assign(i,cards.latitude.second,comp_lat_end);
            }
            for (int i : sp_rect.range_latitude(split_dir)) {
                check_assign(i,cards.longitude.first,comp_long_begin);
                check_assign(i,cards.longitude.second,comp_long_end);
            }
        }

        area.spaces = ArrayView<Space>(
            first.spaces.begin(),
            first.spaces.begin()
                + first.spaces.size()
                + second.spaces.size()
                + halls.size());

        assert(area.verify());
        return area;
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

    AreaData try_split(AreaData area, int const depth) {
        int area_width = area.rect.end_c - area.rect.begin_c;
        int area_height = area.rect.end_r - area.rect.begin_r;

        auto vsplit = make_split_data(room_height_min, area_width, area.rect.begin_r, area.rect.end_r);
        auto hsplit = make_split_data(room_width_min, area_height, area.rect.begin_c, area.rect.end_c);

        // If there's nowhere to split, we cannot create any rooms.
        if (vsplit.range + hsplit.range <= 0) { // TODO: can be equal to 0?
            assert(area.verify());
            return area;
        }

        int split = roll_rng(0, vsplit.range + hsplit.range - 1);
        Dir split_dir = Dir::VERT;

        if (split >= vsplit.range) {
            split_dir = Dir::HORIZ;
            split = split - vsplit.range + hsplit.begin;
        } else {
            split += vsplit.begin;
        }

        area = try_split_recurse(split_dir, area, split, depth);

        assert(area.verify());
        return area;
    }

    Rect make_room_rect(
        int min_long, int max_long,
        int min_lat, int max_lat,
        int center_long, int center_lat,
        Dir free_dir
    ) {
        auto roll_lat_len = roll_rng(
            min_lat,
            min(max_lat, int(max_long/room_ratio_min)));

        const int lat_len = roll_lat_len;
        const int lat_pos = center_lat - lat_len/2;

        auto min_long_len = int(lat_len * room_ratio_min);

        auto roll_long_len = roll_rng(
            max(min_long,min_long_len),
            max_long);

        const int long_len = roll_long_len;
        const int long_pos = center_long - long_len/2;

        Rect rd;
        rd.begin_longitude(free_dir) = long_pos;
        rd.end_longitude(free_dir) = long_pos + long_len;
        rd.begin_latitude(free_dir) = lat_pos;
        rd.end_latitude(free_dir) = lat_pos + lat_len;

        return rd;
    }

    Space make_room(Rect const rect) {
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

    AreaData carve_rooms(AreaData area, int depth) {
        assert(area.verify());

        assert(area.get_view(Cardinal::NORTH).size() == area.rect.width());
        assert(area.get_view(Cardinal::SOUTH).size() == area.rect.width());
        assert(area.get_view(Cardinal::WEST).size() == area.rect.height());
        assert(area.get_view(Cardinal::EAST).size() == area.rect.height());

        // Unless we're at the depth limit, try to split.
        if (depth < depth_max) {
            auto rv = try_split(area, depth);
            assert(rv.verify());
            if (!rv.spaces.empty()) {
                return rv;
            }
        }

        // We've failed to split, so just make a single room.
        auto room = add_space(make_room(area.rect));

        for (int i : number_range(room->data.room.begin_c,room->data.room.end_c)) {
            area.view_from(Cardinal::NORTH,i) = room;
            area.view_from(Cardinal::SOUTH,i) = room;
        }

        for (int i : number_range(room->data.room.begin_r,room->data.room.end_r)) {
            area.view_from(Cardinal::WEST,i) = room;
            area.view_from(Cardinal::EAST,i) = room;
        }

        area.spaces = ArrayView<Space>(room,room+1);

        assert(area.verify());
        return area;
    }

    Space* add_space(Space sp) {
        if (rooms.size() == rooms.capacity()) {
            throw logic_error("Need more space for rooms!");
        }
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

        auto intpow = [](int a, int e){
            int rv = 1;
            for (int i=0; i<e; ++i) {
                rv *= a;
            }
            return rv;
        };

        auto cap = intpow(2,depth_max-1) * 4 - 3;

        rooms.clear();
        rooms.reserve(cap);

        cache_pos = 0;
        cache.resize(max(width,height)*(depth_max-1)*2);

        vector<Space*> mem (width*2 + height*2, nullptr);
        AreaData data;
        data.rect = Rect{0, height, 0, width};
        data.bind_to(ArrayView<Space*>(&mem[0],&mem[mem.size()]));

        auto all = carve_rooms(data, 1);

        assert(&*all.spaces.begin() == &*rooms.begin());

        assert(rooms.capacity() == cap);
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

    SpaceVec const& get_spaces() const {
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
