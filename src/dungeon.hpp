#include <algorithm>
#include <iterator>
#include <list>
#include <utility>
#include <vector>
#include <functional>
#include <random>
#include <stdexcept>

using namespace std;

//Iterator reversor
template <typename Iter>
reverse_iterator<Iter> reverse(Iter i) {
    return reverse_iterator<Iter>(i);
}

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

enum class SpaceType {
    NONE,
    ROOM,
    HALL
};

// Directions
// Horizontal hall - hall going East-West
// Horizontal split - split line going North-South
enum class Dir {
    HORIZ,
    VERT
};

inline Dir flip(Dir dir) {
    switch (dir) {
        case Dir::HORIZ: return Dir::VERT;
        case Dir::VERT: return Dir::HORIZ;
    }
}

struct Rect {
    int begin_r;
    int end_r;
    int begin_c;
    int end_c;

    int width() const {
        return end_c - begin_c;
    }

    int height() const {
        return end_r - begin_r;
    }

    int& begin_longitude(Dir dir) {
        if (dir == Dir::HORIZ) return begin_c;
        return begin_r;
    }

    int& end_longitude(Dir dir) {
        if (dir == Dir::HORIZ) return end_c;
        return end_r;
    }

    int& begin_latitude(Dir dir) {
        if (dir == Dir::HORIZ) return begin_r;
        return begin_c;
    }

    int& end_latitude(Dir dir) {
        if (dir == Dir::HORIZ) return end_r;
        return end_c;
    }

    int begin_longitude(Dir dir) const {
        if (dir == Dir::HORIZ) return begin_c;
        return begin_r;
    }

    int end_longitude(Dir dir) const {
        if (dir == Dir::HORIZ) return end_c;
        return end_r;
    }

    int begin_latitude(Dir dir) const {
        if (dir == Dir::HORIZ) return begin_r;
        return begin_c;
    }

    int end_latitude(Dir dir) const {
        if (dir == Dir::HORIZ) return end_r;
        return end_c;
    }
};

struct HallData {
    Dir dir;
    int dir_loc;
    int begin;
    int end;
};

struct Space {
    SpaceType type = SpaceType::NONE;
    union {
        Rect room;
        HallData hall;
    } data;

    vector<Space*> neighbors;
};

inline Rect get_shape(Space const& sp) {
    Rect rv;

    switch (sp.type) {
        case SpaceType::ROOM: {
            rv = sp.data.room;
        } break;

        case SpaceType::HALL: {
            Dir dir = sp.data.hall.dir;
            rv.begin_longitude(dir) = sp.data.hall.begin;
            rv.end_longitude(dir) = sp.data.hall.end;
            rv.begin_latitude(dir) = sp.data.hall.dir_loc;
            rv.end_latitude(dir) = sp.data.hall.dir_loc+1;
        } break;

        case SpaceType::NONE: {
            throw logic_error("Space must be ROOM or HALL.");
        } break;
    }

    return rv;
}

template <typename SpaceVec, typename GetLong, typename Comp>
vector<Space*> get_partition_data(SpaceVec& spaces, Dir dir, int lat_begin, int lat_end, GetLong get_long, Comp comp) {
    vector<Space*> rv (lat_end - lat_begin, nullptr);
    for (Space* sp : spaces) {
        auto shape = get_shape(*sp);
        for (int i = shape.begin_latitude(dir), ie = shape.end_latitude(dir); i<ie; ++i) {
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

    mt19937 rng {random_device{}()};

    Space* carve_hallway(vector<Space*> const& first, vector<Space*> const& second, Dir dir, int lat_begin, int lat_end) {
        Space* rv;

        auto first_data  = get_partition_data(first,  dir, lat_begin, lat_end, [=](const auto& v){return v.end_longitude(dir);}, greater<void>{});
        auto second_data = get_partition_data(second, dir, lat_begin, lat_end, [=](const auto& v){return v.begin_longitude(dir);},  less<void>{});

        auto first_bounds  = find_bounds_if(begin(first_data),  end(first_data),  is_not_null);
        auto second_bounds = find_bounds_if(begin(second_data), end(second_data), is_not_null);

        int a = max(
            first_bounds.first  - begin(first_data),
            second_bounds.first - begin(second_data));

        int b = min(
            first_bounds.second  - begin(first_data),
            second_bounds.second - begin(second_data));

        uniform_int_distribution<int> roll (a,b-1);
        int loc = roll(rng);

        auto  first_ptr =  first_data[loc-lat_begin];
        auto second_ptr = second_data[loc-lat_begin];

        Space space;
        HallData hd;

        space.type = SpaceType::HALL;
        hd.dir = dir;
        hd.dir_loc = loc;
        hd.begin = get_shape(*first_ptr).end_longitude(dir);
        hd.end = get_shape(*second_ptr).begin_longitude(dir);
        space.data.hall = hd;
        space.neighbors.push_back(first_ptr);
        space.neighbors.push_back(second_ptr);

        // ???
        rooms.push_back(move(space));
        rv = &rooms.back();
        for (auto& rptr : rv->neighbors) {
            rptr->neighbors.push_back(rv);
        }

        return rv;
    }

    // Small function to avoid code duplication.
    template <typename CarveFunc>
    vector<Space*> try_split_recurse(Dir split_dir, Rect rect, int pos, CarveFunc carve) {
        vector<Space*> rv;
        auto first = carve(rect.begin_longitude(split_dir), pos);
        auto second = carve(pos, rect.end_longitude(split_dir));
        auto hall = carve_hallway(first, second, split_dir, rect.begin_latitude(split_dir), rect.end_latitude(split_dir));
        rv.reserve(first.size() + second.size() + 1);
        move(
            begin(first),
            end(first),
            back_inserter(rv));
        move(
            begin(second),
            end(second),
            back_inserter(rv));
        rv.push_back(hall);
        return rv;
    }

    vector<Space*> try_split(Rect rect, int depth) {
        vector<Space*> rv;

        struct SplitData {
            int min;
            int begin;
            int end;
            int range;
        };

        // Essentially a scope-capturing constructor for SplitData.
        auto make_split_data = [&](int min_len, int area_size, int begin, int end) {
            SplitData rv;
            rv.min = max(
                min_len + 1, // +1 to give room for hallways.
                int(area_size * room_ratio_min));
            rv.begin = begin + rv.min;
            rv.end = end - rv.min + 1;
            rv.range = max(
                rv.end - rv.begin,
                0);
            return rv;
        };

        int area_width = rect.end_c - rect.begin_c;
        int area_height = rect.end_r - rect.begin_r;

        const auto vsplit = make_split_data(room_height_min, area_width, rect.begin_r, rect.end_r);
        const auto hsplit = make_split_data(room_width_min, area_height, rect.begin_c, rect.end_c);

        // If there's nowhere to split, we cannot create any rooms.
        if (vsplit.range + hsplit.range <= 0) {
            return rv;
        }

        uniform_int_distribution<int> roll (0, vsplit.range + hsplit.range - 1);

        int split = roll(rng);
        Dir split_dir = Dir::VERT;

        if (split >= vsplit.range) {
            split_dir = Dir::HORIZ;
            split = split - vsplit.range;
        }

        switch (split_dir) {
            case Dir::VERT: {
                auto carve = [&](int begin, int end) {
                    return carve_rooms(Rect{begin, end, rect.begin_c, rect.end_c}, depth+1);
                };
                rv = try_split_recurse(split_dir, rect, vsplit.begin + split, carve);
            } break;

            case Dir::HORIZ: {
                auto carve = [&](int begin, int end) {
                    return carve_rooms(Rect{rect.begin_r, rect.end_r, begin, end}, depth+1);
                };
                rv = try_split_recurse(split_dir, rect, hsplit.begin + split, carve);
            } break;
        }

        return rv;
    }

    vector<Space*> carve_rooms(Rect rect, int depth) {
        vector<Space*> rv;

        // Unless we're at the depth limit, try to split.
        if (depth < depth_max) {
            rv = try_split(rect, depth);
            if (!rv.empty()) {
                return rv;
            }
        }

        // We've failed to split, so just make a single room.

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

        auto make_room = [&](
            int min_long, int max_long,
            int min_lat, int max_lat,
            int center_long, int center_lat
        ) {
            uniform_int_distribution<int> roll_lat_len (
                min_lat,
                max_lat);

            const int lat_len = roll_lat_len(rng);
            const int lat_pos = center_lat - lat_len/2;

            uniform_int_distribution<int> roll_long_len (
                max(
                    min_long,
                    int(lat_len * room_ratio_min)),
                max_long);

            const int long_len = roll_long_len(rng);
            const int long_pos = center_long - long_len/2;

            Rect rd;
            rd.begin_longitude(free_dir) = long_pos;
            rd.end_longitude(free_dir) = long_pos + long_len;
            rd.begin_latitude(free_dir) = lat_pos;
            rd.end_latitude(free_dir) = lat_pos + lat_len;

            return rd;
        };

        Space room;
        room.type = SpaceType::ROOM;

        switch (free_dir) {
            case Dir::HORIZ: {
                room.data.room = make_room(
                    room_width_min, area_width,
                    room_height_min, area_height,
                    center_c, center_r);
            } break;

            case Dir::VERT: {
                room.data.room = make_room(
                    room_height_min, area_height,
                    room_width_min, area_width,
                    center_r, center_c);
            } break;
        }

        rooms.push_back(move(room));
        rv.push_back(&rooms.back());

        return rv;
    }

public:

    void go(int w, int h) {
        width = w;
        height = h;

        rooms.clear();

        carve_rooms(Rect{0, height, 0, width}, 0);
    }

    template <typename Out>
    void print(Out& out) {
        out << "graph g {" << endl;
        for (Space& s : rooms) {
            Space* sp = &s;
            auto rect = get_shape(s);
            out << "    " << intptr_t(sp) << " [label=\""
                << (s.type==SpaceType::ROOM? "Room " : "Hall ")
                << rect.begin_r << "-" << rect.end_r << ":"
                << rect.begin_c << "-" << rect.end_c
                << "\"];" << endl;
            for (Space* sp2 : s.neighbors) {
                if (less<void>{}(sp, sp2)) {
                    out << "    " << intptr_t(sp) << " -- " << intptr_t(sp2) << ";" << endl;
                }
            }
        }
        out << "}" << endl;
    }
};
