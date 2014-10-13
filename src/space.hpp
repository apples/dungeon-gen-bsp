#ifndef SPACE_HPP
#define SPACE_HPP

#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

// Directions
// Horizontal hall - hall going East-West
// Horizontal split - split line going North-South
enum class Dir {
    NONE,
    HORIZ,
    VERT
};

inline std::ostream& operator<<(std::ostream& out, Dir const& dir) {
    switch (dir) {
        case Dir::NONE: {
            out << "Dir::NONE";
        } break;
        case Dir::HORIZ: {
            out << "Dir::HORIZ";
        } break;
        case Dir::VERT: {
            out << "Dir::VERT";
        } break;
    }
    return out;
}

inline Dir flip(Dir dir) {
    switch (dir) {
        case Dir::HORIZ: return Dir::VERT;
        case Dir::VERT: return Dir::HORIZ;
        default: return Dir::NONE;
    }
}

struct Rect {
    int begin_r = -1;
    int end_r = -1;
    int begin_c = -1;
    int end_c = -1;

    Rect() = default;
    Rect(int br, int er, int bc, int ec)
        : begin_r(br), end_r(er), begin_c(bc), end_c(ec) {}

    int width() const {
        return end_c - begin_c;
    }

    int height() const {
        return end_r - begin_r;
    }

    int center_r() const {
        return (begin_r+end_r)/2;
    }

    int center_c() const {
        return (begin_c+end_c)/2;
    }

    int& begin_longitude(Dir dir) {
        Rect const& self = *this;
        return const_cast<int&>(self.begin_longitude(dir));
    }

    int& end_longitude(Dir dir) {
        Rect const& self = *this;
        return const_cast<int&>(self.end_longitude(dir));
    }

    int& begin_latitude(Dir dir) {
        Rect const& self = *this;
        return const_cast<int&>(self.begin_latitude(dir));
    }

    int& end_latitude(Dir dir) {
        Rect const& self = *this;
        return const_cast<int&>(self.end_latitude(dir));
    }

    int const& begin_longitude(Dir dir) const {
        switch (dir) {
            case Dir::HORIZ: return begin_c;
            case Dir::VERT:  return begin_r;
            default: throw std::logic_error("Must be valid direction!");
        }
    }

    int const& end_longitude(Dir dir) const {
        switch (dir) {
            case Dir::HORIZ: return end_c;
            case Dir::VERT:  return end_r;
            default: throw std::logic_error("Must be valid direction!");
        }
    }

    int const& begin_latitude(Dir dir) const {
        switch (dir) {
            case Dir::HORIZ: return begin_r;
            case Dir::VERT:  return begin_c;
            default: throw std::logic_error("Must be valid direction!");
        }
    }

    int const& end_latitude(Dir dir) const {
        switch (dir) {
            case Dir::HORIZ: return end_r;
            case Dir::VERT:  return end_c;
            default: throw std::logic_error("Must be valid direction!");
        }
    }

    std::pair<Rect,Rect> split(Dir dir, int pos) const {
        std::pair<Rect,Rect> rv (*this,*this);
        rv.first.end_longitude(dir) = pos;
        rv.second.begin_longitude(dir) = pos;
        return rv;
    }
};

inline std::ostream& operator<<(std::ostream& out, Rect const& rect) {
    out << "Rect{"
        << rect.begin_r << ", "
        << rect.end_r << "; "
        << rect.begin_c << ", "
        << rect.end_c
        << "}";
    return out;
}

struct HallData {
    Dir dir = Dir::NONE;
    int dir_loc = -1;
    int begin = -1;
    int end = -1;
    int thickness = 0;
};

struct NoData {
    const char str[12] = "BAD INIT!";
};

enum class SpaceType {
    NONE,
    ROOM,
    HALL
};

struct Space {
    SpaceType type = SpaceType::NONE;
    union SpaceData {
        NoData nd;
        Rect room;
        HallData hall;
        SpaceData() : nd{} {}
    } data;

    std::vector<Space*> neighbors;

    Space() {
        neighbors.reserve(5);
    }
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
            rv.end_latitude(dir) = sp.data.hall.dir_loc+sp.data.hall.thickness;
        } break;

        default: {
            throw std::logic_error("Space must be ROOM or HALL.");
        } break;
    }

    return rv;
}

#endif // SPACE_HPP
