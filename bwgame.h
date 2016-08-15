
struct unit_id {
	uint16_t raw_value = 0;
	unit_id() = default;
	explicit unit_id(uint16_t raw_value) : raw_value(raw_value) {}
	explicit unit_id(size_t index, int generation) : raw_value((uint16_t)(index | generation << 11)) {}
	size_t index() {
		return raw_value & 0x7ff;
	}
	int generation() {
		return raw_value >> 11;
	}
};

template<size_t bits>
using int_fastn_t = typename std::conditional<bits <= 8, std::int_fast8_t,
	typename std::conditional<bits <= 16, int_fast16_t,
	typename std::conditional<bits <= 32, int_fast32_t,
	typename std::enable_if<bits <= 64, int_fast64_t>::type>::type>::type>::type;
template<size_t bits>
using uint_fastn_t = typename std::conditional<bits <= 8, std::uint_fast8_t,
	typename std::conditional<bits <= 16, uint_fast16_t,
	typename std::conditional<bits <= 32, uint_fast32_t,
	typename std::enable_if<bits <= 64, uint_fast64_t>::type>::type>::type>::type;

template<typename T, std::enable_if<std::is_integral<T>::value && std::numeric_limits<T>::radix == 2>* = nullptr>
using int_bits = std::integral_constant<size_t, std::numeric_limits<T>::digits + std::is_signed<T>::value>;

template<size_t integer_bits, size_t fractional_bits, bool is_signed, bool exact_integer_bits = false>
struct fixed_point {
	static const bool is_signed = is_signed;
	static const bool exact_integer_bits = exact_integer_bits;
	static const size_t integer_bits = integer_bits;
	static const size_t fractional_bits = fractional_bits;
	static const size_t total_bits = integer_bits + fractional_bits;
	using raw_unsigned_type = uint_fastn_t<total_bits>;
	using raw_signed_type = int_fastn_t<total_bits>;
	using raw_type = typename std::conditional<is_signed, raw_signed_type, raw_unsigned_type>::type;
	raw_type raw_value;

	using double_size_fixed_point = fixed_point<total_bits * 2 - fractional_bits, fractional_bits, is_signed>;

	void wrap() {
		if (!exact_integer_bits) return;
		raw_value <<= int_bits<raw_type>::value - total_bits;
		raw_value >>= int_bits<raw_type>::value - total_bits;
	}

	static fixed_point from_raw(raw_type raw_value) {
		fixed_point r;
		r.raw_value = raw_value;
		r.wrap();
		return r;
	}

	template<typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
	static fixed_point integer(T integer_value) {
		return from_raw((raw_type)integer_value << fractional_bits);
	}

	static fixed_point zero() {
		return integer(0);
	}
	static fixed_point one() {
		return integer(1);
	}

	raw_type integer_part() const {
		return raw_value >> fractional_bits;
	}
	raw_type fractional_part() const {
		return raw_value & (((raw_type)1 << fractional_bits) - 1);
	}

	template<size_t n_integer_bits, bool n_exact_integer_bits, size_t result_integer_bits = integer_bits, typename std::enable_if<(n_integer_bits > result_integer_bits)>::type* = nullptr>
	static auto truncate(const fixed_point<n_integer_bits, fractional_bits, is_signed, n_exact_integer_bits>& n) {
		return fixed_point<result_integer_bits, fractional_bits, is_signed, exact_integer_bits>::from_raw((raw_type)n.raw_value);
	}

	template<size_t n_integer_bits, bool n_exact_integer_bits, size_t result_integer_bits = integer_bits, typename std::enable_if<(result_integer_bits > n_integer_bits)>::type* = nullptr >
	static auto extend(const fixed_point<n_integer_bits, fractional_bits, is_signed, n_exact_integer_bits>& n) {
		return fixed_point<result_integer_bits, fractional_bits, is_signed, exact_integer_bits>::from_raw((raw_type)n.raw_value);
	}

	fixed_point floor() const {
		return integer(integer_part());
	}
	fixed_point ceil() const {
		return (*this + integer(1) - from_raw(1)).floor();
	}
	fixed_point abs() const {
		if (*this >= zero()) return *this;
		else return from_raw(-raw_value);
	}

	auto as_signed() const {
		return fixed_point<integer_bits + (is_signed ? 0 : 1), fractional_bits, true, exact_integer_bits>::from_raw(raw_value);
	}
	auto as_unsigned() const {
		return fixed_point<integer_bits, fractional_bits, false, exact_integer_bits>::from_raw(abs());
	}

	bool operator==(const fixed_point& n) const {
		return raw_value == n.raw_value;
	}
	bool operator!=(const fixed_point& n) const {
		return raw_value != n.raw_value;
	}
	bool operator<(const fixed_point& n) const {
		return raw_value < n.raw_value;
	}
	bool operator<=(const fixed_point& n) const {
		return raw_value <= n.raw_value;
	}
	bool operator>(const fixed_point& n) const {
		return raw_value > n.raw_value;
	}
	bool operator>=(const fixed_point& n) const {
		return raw_value >= n.raw_value;
	}

	fixed_point operator-() const {
		static_assert(is_signed, "fixed_point: cannot negate an unsigned number");
		return from_raw(-raw_value);
	}

	fixed_point& operator+=(const fixed_point& n) {
		raw_value += n.raw_value;
		wrap();
		return *this;
	}
	fixed_point operator+(const fixed_point& n) const {
		return from_raw(raw_value + n.raw_value);
	}
	fixed_point& operator-=(const fixed_point& n) {
		raw_value -= n.raw_value;
		wrap();
		return *this;
	}
	fixed_point operator-(const fixed_point& n) const {
		return from_raw(raw_value - n.raw_value);
	}
	template<typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
	fixed_point operator/=(T integer_value) {
		static_assert(std::is_signed<T>::value == is_signed, "fixed_point: cannot mix signed/unsigned in division");
		raw_value /= integer_value;
		wrap();
		return *this;
	}
	template<typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
	fixed_point operator/(T integer_value) const {
		static_assert(std::is_signed<T>::value == is_signed, "fixed_point: cannot mix signed/unsigned in division");
		return from_raw(raw_value / integer_value);
	}
	template<typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
	fixed_point operator*=(T integer_value) {
		static_assert(std::is_signed<T>::value == is_signed, "fixed_point: cannot mix signed/unsigned in multiplication");
		raw_value *= integer_value;
		wrap();
		return *this;
	}
	template<typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
	auto operator*(T integer_value) const {
		static_assert(std::is_signed<T>::value == is_signed, "fixed_point: cannot mix signed/unsigned in multiplication");
		using result_type = fixed_point<integer_bits + int_bits<T>::value, fractional_bits, is_signed, exact_integer_bits>;
		return result_type::from_raw((result_type::raw_type)raw_value * integer_value);
	}

	template<size_t n_integer_bits>
	auto operator*=(const fixed_point<n_integer_bits, fractional_bits, is_signed, exact_integer_bits>& n) {
		return *this = truncate(*this * n);
	}

	template<size_t n_integer_bits>
	auto operator/=(const fixed_point<n_integer_bits, fractional_bits, is_signed, exact_integer_bits>& n) const {
		return *this = truncate(*this / n);
	}

	template<size_t n_integer_bits>
	auto operator*(const fixed_point<n_integer_bits, fractional_bits, is_signed, exact_integer_bits>& n) {
		using result_type = fixed_point<integer_bits + n_integer_bits, fractional_bits, is_signed, exact_integer_bits>;
		constexpr size_t tmp_bits = integer_bits + n_integer_bits + fractional_bits;
		using tmp_t = std::conditional<is_signed, int_fastn_t<tmp_bits>, uint_fastn_t<tmp_bits>>::type;
		tmp_t tmp = ((tmp_t)raw_value * (tmp_t)n.raw_value) >> fractional_bits;
		return result_type::from_raw((result_type::raw_type)tmp);
	}

	template<size_t n_integer_bits>
	auto operator/(const fixed_point<n_integer_bits, fractional_bits, is_signed, exact_integer_bits>& n) const {
		constexpr size_t result_integer_bits = integer_bits + fractional_bits;
		using result_type = fixed_point<result_integer_bits, fractional_bits, is_signed, exact_integer_bits>;
		constexpr size_t result_bits = result_integer_bits + fractional_bits;
		constexpr size_t n_bits = n_integer_bits + fractional_bits;
		constexpr size_t tmp_bits = result_bits > n_bits ? result_bits : n_bits;
		using tmp_t = std::conditional<is_signed, int_fastn_t<tmp_bits>, uint_fastn_t<tmp_bits>>::type;
		tmp_t tmp = ((tmp_t)raw_value << fractional_bits) / (tmp_t)n.raw_value;
		return result_type::from_raw((result_type::raw_type)tmp);
	}

};

using fp8 = fixed_point<24, 8, true>;
using ufp8 = fixed_point<24, 8, false>;

using direction_t = fixed_point<0, 8, true, true>;

#include "bwenums.h"

namespace bwgame {
;

#include "util.h"

using xy_fp8 = xy_t<fp8>;

#include "data_types.h"
#include "game_types.h"

#include "data_loading.h"

static const std::array<unsigned int, 64> arctan_inv_table = {
	7, 13, 19, 26, 32, 38, 45, 51, 58, 65, 71, 78, 85, 92,
	99, 107, 114, 122, 129, 137, 146, 154, 163, 172, 181,
	190, 200, 211, 221, 233, 244, 256, 269, 283, 297, 312,
	329, 346, 364, 384, 405, 428, 452, 479, 509, 542, 578,
	619, 664, 716, 775, 844, 926, 1023, 1141, 1287, 1476,
	1726, 2076, 2600, 3471, 5211, 10429, std::numeric_limits<unsigned int>::max()
};

// Broodwar linked lists insert new elements between the first and second entry.
template<typename cont_T, typename T>
void bw_insert_list(cont_T&cont, T&v) {
	if (cont.empty()) cont.push_front(v);
	else cont.insert(++cont.begin(), v);
}

struct sight_values_t {
	struct maskdat_node_t;
	typedef a_vector<maskdat_node_t> maskdat_t;
	struct maskdat_node_t {
		// 
		//  I would like to change this structure a bit, move vision_propagation to a temporary inside reveal_sight_at,
		//  and change prev_count to a bool since it can only have two values, or remove it entirely.
		//
		// TODO: remove vision_propagation, since this struct is supposed to be static (stored in game_state)
		//
		maskdat_node_t*prev; // the tile from us directly towards the origin (diagonal is allowed and preferred)
		// the other tile with equal diagonal distance to the origin as prev, if it exists.
		// otherwise, it is prev
		maskdat_node_t*prev2;
		size_t map_index_offset;
		// temporary variable used when spreading vision to make sure we don't go through obstacles
		mutable uint32_t vision_propagation;
		int8_t x;
		int8_t y;
		// prev_count will be 1 if prev and prev2 are equal, otherwise it is 2
		int8_t prev_count;
	};
	static_assert(sizeof(maskdat_node_t) == 20, "maskdat_node_t: wrong size");

	int max_width, max_height;
	int min_width, min_height;
	int min_mask_size;
	int ext_masked_count;
	maskdat_t maskdat;

};

struct cv5_entry {
	uint16_t field_0;
	uint16_t flags;
	uint16_t left;
	uint16_t top;
	uint16_t right;
	uint16_t bottom;
	uint16_t field_C;
	uint16_t field_E;
	uint16_t field_10;
	uint16_t field_12;
	std::array<uint16_t, 16> megaTileRef;
};
static_assert(sizeof(cv5_entry) == 52, "cv5_entry: wrong size");
struct vf4_entry {
	std::array<uint16_t, 16> flags;
};
static_assert(sizeof(vf4_entry) == 32, "vf4_entry: wrong size");
struct vx4_entry {
	std::array<uint16_t, 16> images;
};
static_assert(sizeof(vx4_entry) == 32, "vx4_entry: wrong size");
struct vr4_entry {
	std::array<uint8_t, 64> bitmap;
};
static_assert(sizeof(vr4_entry) == 64, "vr4_entry: wrong size");

struct tile_id {
	uint16_t raw_value = 0;
	tile_id() = default;
	explicit tile_id(uint16_t raw_value) : raw_value(raw_value) {}
	explicit tile_id(size_t group_index, size_t subtile_index) : raw_value((uint16_t)(group_index << 4 | subtile_index)) {}
	bool has_creep() {
		return ((raw_value >> 4) & 0x8000) != 0;
	}
	size_t group_index() {
		return (raw_value >> 4) & 0x7ff;
	}
	size_t subtile_index() {
		return raw_value & 0xf;
	}
	explicit operator bool() const {
		return raw_value != 0;
	}
};

struct tile_t {
	enum {
		flag_walkable = 1,
		flag_unk0 = 2,
		flag_unwalkable = 4,
		flag_unk1 = 8,
		flag_unk2 = 0x10,
		flag_unk3 = 0x20,
		flag_has_creep = 0x40,
		flag_unbuildable = 0x80,
		flag_very_high = 0x100,
		flag_middle = 0x200,
		flag_high = 0x400,
		flag_occupied = 0x800,
		flag_creep_receding = 0x1000,
		flag_partially_walkable = 0x2000,
		flag_temporary_creep = 0x4000,
		flag_unk4 = 0x8000
	};
	union {
		struct {
			uint8_t visible;
			uint8_t explored;
			uint16_t flags;
		};
		uint32_t raw;
	};
	operator uint32_t() const {
		return raw;
	}
	bool operator==(uint32_t val) const {
		return raw == val;
	}
};

struct paths_t {

	struct region {
		uint16_t flags = 0x1FFD;
		size_t index = ~(size_t)0;
		xy_t<size_t> tile_center;
		rect_t<xy_t<size_t>> tile_area;
		xy_t<fp8> center;
		rect area;
		size_t tile_count = 0;
		size_t group_index = 0;
		a_vector<region*> walkable_neighbors;
		a_vector<region*> non_walkable_neighbors;
		int priority = 0;

		bool walkable() const {
			return flags != 0x1ffd;
		}
	};

	struct split_region {
		uint16_t mask;
		region* a;
		region* b;
	};

	struct contour {
		std::array<int, 3> v;
		size_t dir;
		uint8_t flags;
	};

	// tile_region_index values -
	//  [0, 5000) index into regions
	//  [5000, 0x2000) unmapped (0x1ffd unwalkable, otherwise walkable)
	//  [0x2000, ...] index + 0x2000 into split_regions
	a_vector<size_t> tile_region_index = a_vector<size_t>(256 * 256);

	rect_t<xy_t<size_t>> tile_bounding_box;

	a_vector<region> regions;

	a_vector<split_region> split_regions;

	std::array<a_vector<contour>, 4> contours;

	region* get_new_region() {
		if (regions.capacity() != 5000) regions.reserve(5000);
		if (regions.size() >= 5000) xcept("too many regions");
		regions.emplace_back();
		region* r = &regions.back();
		r->index = regions.size() - 1;
		return r;
	}

	region* get_region_at(xy pos) {
		size_t index = tile_region_index.at((size_t)pos.y / 32 * 256 + (size_t)pos.x / 32);
		if (index >= 0x2000) {
			size_t mask_index = ((size_t)pos.y / 8 & 3) * 4 + ((size_t)pos.x / 8 & 3);
			auto* split = &split_regions[index - 0x2000];
			if (split->mask & (1 << mask_index)) return split->a;
			else return split->b;
		} else return &regions[index];
	}

};

struct global_state {

	global_state() = default;
	global_state(global_state&) = delete;
	global_state(global_state&&) = default;
	global_state& operator=(global_state&) = delete;
	global_state& operator=(global_state&&) = default;

	flingy_types_t flingy_types;
	sprite_types_t sprite_types;
	image_types_t image_types;
	order_types_t order_types;
	iscript_t iscript;

	a_vector<grp_t> grps;
	a_vector<grp_t*> image_grp;
	a_vector<a_vector<a_vector<xy>>> lo_offsets;
	a_vector<std::array<a_vector<a_vector<xy>>*, 6>> image_lo_offsets;

	std::array<xy_fp8, 256> direction_table;
};

struct game_state {

	game_state() = default;
	game_state(game_state&) = delete;
	game_state(game_state&&) = default;
	game_state& operator=(game_state&) = delete;
	game_state& operator=(game_state&&) = default;

	size_t map_tile_width;
	size_t map_tile_height;
	size_t map_walk_width;
	size_t map_walk_height;
	size_t map_width;
	size_t map_height;

	a_string map_file_name;

	a_vector<a_string> map_strings;
	a_string get_string(size_t index) {
		if (index == 0) return "<null string>";
		--index;
		if (index >= map_strings.size()) return "<invalid string index>";
		return map_strings[index];
	}

	a_string scenario_name;
	a_string scenario_description;

	std::array<int, 228> unit_air_strength;
	std::array<int, 228> unit_ground_strength;

	struct force_t {
		a_string name;
		uint8_t flags;
	};
	std::array<force_t, 4> forces;

	std::array<sight_values_t, 12> sight_values;

	size_t tileset_index;

	//a_vector<tile_id> gfx_creep_tiles;
	a_vector<tile_id> gfx_tiles;
	a_vector<cv5_entry> cv5;
	a_vector<vf4_entry> vf4;
	a_vector<uint16_t> mega_tile_flags;

	unit_types_t unit_types;
	weapon_types_t weapon_types;
	upgrade_types_t upgrade_types;
	tech_types_t tech_types;

	std::array<std::array<bool, 228>, 12> unit_type_allowed;
	std::array<std::array<int, 61>, 12> max_upgrade_levels;
	std::array<std::array<bool, 44>, 12> tech_available;

	std::array<xy, 12> start_locations;

	bool is_replay;
	int local_player;

	int max_unit_width;
	int max_unit_height;
};

struct state_base_copyable {

	const global_state* global;
	game_state* game;

	int update_tiles_countdown;

	std::array<int, 12> selection_circle_color;

	int order_timer_counter;
	int secondary_order_timer_counter;

	struct player_t {
		enum {
			controller_inactive,
			controller_computer_game,
			controller_occupied,
			controller_rescue_passive,
			controller_unused_rescue_active,
			controller_computer,
			controller_open,
			controller_neutral,
			controller_closed,
			controller_unused_observer,
			controller_user_left,
			controller_computer_defeated
		};
		int controller;
		int race;
		int force;
	};
	std::array<player_t, 12> players;

	std::array<std::array<int, 12>, 12> alliances;

	std::array<std::array<int, 61>, 12> upgrade_levels;
	std::array<std::array<bool, 44>, 12> tech_researched;

	std::array<std::array<int, 228>, 12> unit_counts;
	std::array<std::array<int, 228>, 12> completed_unit_counts;

	std::array<int, 12> factory_counts;
	std::array<int, 12> building_counts;
	std::array<int, 12> non_building_counts;

	std::array<int, 12> completed_factory_counts;
	std::array<int, 12> completed_building_counts;
	std::array<int, 12> completed_non_building_counts;

	std::array<int, 12> total_buildings_ever_completed;
	std::array<int, 12> total_non_buildings_ever_completed;

	std::array<int, 12> unit_score;
	std::array<int, 12> building_score;

	std::array<std::array<int, 12>, 3> supply_used;
	std::array<std::array<int, 12>, 3> supply_available;

	uint32_t local_mask;

	std::array<int, 12> shared_vision;

	a_vector<tile_id> gfx_creep_tiles;
	a_vector<tile_t> tiles;
	a_vector<uint16_t> tiles_mega_tile_index;

	std::array<int, 0x100> random_counts;
	int total_random_counts;
	uint32_t lcg_rand_state;

	int last_net_error;

	rect viewport;

	size_t allocated_order_count;
};

struct state_base_non_copyable {

	state_base_non_copyable() = default;
	state_base_non_copyable(state_base_non_copyable&) = delete;
	state_base_non_copyable(state_base_non_copyable&&) = default;
	state_base_non_copyable& operator=(state_base_non_copyable&) = delete;
	state_base_non_copyable& operator=(state_base_non_copyable&&) = default;

	intrusive_list<unit_t, default_link_f> visible_units;
	intrusive_list<unit_t, default_link_f> hidden_units;
	intrusive_list<unit_t, default_link_f> scanner_sweep_units;
	intrusive_list<unit_t, default_link_f> sight_related_units;
	intrusive_list<unit_t, default_link_f> free_units;

	a_vector<unit_t> units = a_vector<unit_t>(1700);

	std::array<intrusive_list<unit_t, intrusive_list_member_link<unit_t, &unit_t::player_units_link>>, 12> player_units;

	a_vector<intrusive_list<sprite_t, default_link_f>> sprites_on_tile_line;
	intrusive_list<sprite_t, default_link_f> free_sprites;
	a_vector<sprite_t> sprites = a_vector<sprite_t>(2500);

	intrusive_list<image_t, default_link_f> free_images;
	a_vector<image_t> images = a_vector<image_t>(5000);

	intrusive_list<order_t, default_link_f> free_orders;
	a_vector<order_t> orders = a_vector<order_t>(2000);

	static const size_t unit_finder_group_size = 32;
	struct unit_finder_entry {
		unit_t* u;
		int value;
		std::pair<unit_finder_entry*, unit_finder_entry*> link;
	};
	a_vector<a_vector<unit_finder_entry>> unit_finder_groups;
	intrusive_list<unit_finder_entry, default_link_f> unit_finder_list;
	using unit_finder_list_iterator = decltype(unit_finder_list)::iterator;

	paths_t paths;
};

struct state : state_base_copyable, state_base_non_copyable {
};

struct state_functions {

	state& st;
	const global_state& global_st = *st.global;
	const game_state& game_st = *st.game;

	explicit state_functions(state&st) : st(st) {}

	bool allow_random = false;
	bool update_tiles = false;
	unit_t* iscript_unit = nullptr;
	unit_t* iscript_order_unit = nullptr;
	mutable bool unit_finder_search_active = false;

	const order_type_t* get_order_type(int id) const {
		if ((size_t)id >= 189) xcept("invalid order id %d", id);
		return &global_st.order_types.vec[id];
	}

	struct iscript_unit_setter {
		unit_t*& iscript_unit;
		unit_t* prev_iscript_unit;
		iscript_unit_setter(state_functions* sf, unit_t* new_iscript_unit) : iscript_unit(sf->iscript_unit) {
			prev_iscript_unit = iscript_unit;
			iscript_unit = new_iscript_unit;
		}
		~iscript_unit_setter() {
			iscript_unit = prev_iscript_unit;
		}
	};

	void u_set_status_flag(unit_t* u, unit_t::status_flags_t flag) {
		u->status_flags |= flag;
	};
	void u_unset_status_flag(unit_t* u, unit_t::status_flags_t flag) {
		u->status_flags &= ~flag;
	};

	void u_set_status_flag(unit_t* u, unit_t::status_flags_t flag, bool value) {
		if (value) u->status_flags |= flag;
		else u->status_flags &= ~flag;
	};
	
	void u_set_movement_flag(unit_t* u, int flag) {
		u->movement_flags |= flag;
	}
	void u_unset_movement_flag(unit_t* u, int flag) {
		u->movement_flags &= ~flag;
	}

	bool ut_flag(const unit_t* u, unit_type_t::flags_t flag) const {
		return (u->unit_type->flags & flag) != 0;
	};
	bool u_status_flag(const unit_t* u, unit_t::status_flags_t flag) const {
		return (u->status_flags & flag) != 0;
	};
	bool us_flag(const unit_t* u, sprite_t::flags_t flag) const {
		return (u->sprite->flags & flag) != 0;
	}
	bool s_flag(const sprite_t* s, sprite_t::flags_t flag) const {
		return (s->flags & flag) != 0;
	}
	bool u_movement_flag(const unit_t* u, int flag) const {
		return (u->movement_flags & flag) != 0;
	}

	bool u_completed(const unit_t* u) const {
		return u_status_flag(u, unit_t::status_flag_completed);
	};
	bool u_in_building(const unit_t* u) const {
		return u_status_flag(u, unit_t::status_flag_in_building);
	};
	bool u_immovable(const unit_t* u) const {
		return u_status_flag(u, unit_t::status_flag_immovable);
	};
	bool u_disabled(const unit_t* u) const {
		return u_status_flag(u, unit_t::status_flag_disabled);
	};
	bool u_burrowed(const unit_t* u) const {
		return u_status_flag(u, unit_t::status_flag_burrowed);
	};
	bool u_can_turn(const unit_t* u) const {
		return u_status_flag(u, unit_t::status_flag_can_turn);
	}
	bool u_can_move(const unit_t* u) const {
		return u_status_flag(u, unit_t::status_flag_can_move);
	}
	bool u_grounded_building(const unit_t* u) const {
		return u_status_flag(u, unit_t::status_flag_grounded_building);
	};
	bool u_hallucination(const unit_t* u) const {
		return u_status_flag(u, unit_t::status_flag_hallucination);
	};
	bool u_flying(const unit_t* u) const {
		return u_status_flag(u, unit_t::status_flag_flying);
	};
	bool u_speed_upgrade(const unit_t* u) const {
		return u_status_flag(u, unit_t::status_flag_speed_upgrade);
	};
	bool u_cooldown_upgrade(const unit_t* u) const {
		return u_status_flag(u, unit_t::status_flag_cooldown_upgrade);
	};
	bool u_gathering(const unit_t* u) const {
		return u_status_flag(u, unit_t::status_flag_gathering);
	}
	bool u_requires_detector(const unit_t* u) const {
		return u_status_flag(u, unit_t::status_flag_requires_detector);
	}
	bool u_cloaked(const unit_t* u) const {
		return u_status_flag(u, unit_t::status_flag_cloaked);
	}
	bool u_frozen(const unit_t* u) const {
		return u_status_flag(u, unit_t::status_flag_frozen);
	}
	bool u_cannot_attack(const unit_t* u) const {
		return u_status_flag(u, unit_t::status_flag_cannot_attack);
	}
	bool u_order_not_interruptible(const unit_t* u) const {
		return u_status_flag(u, unit_t::status_flag_order_not_interruptible);
	}
	bool u_iscript_nobrk(const unit_t* u) const {
		return u_status_flag(u, unit_t::status_flag_iscript_nobrk);
	}
	bool u_collision(const unit_t* u) const {
		return u_status_flag(u, unit_t::status_flag_collision);
	}
	bool u_ground_unit(const unit_t* u) const {
		return u_status_flag(u, unit_t::status_flag_ground_unit);
	}
	bool u_no_collide(const unit_t* u) const {
		return u_status_flag(u, unit_t::status_flag_no_collide);
	}
	bool u_invincible(const unit_t* u) const {
		return u_status_flag(u, unit_t::status_flag_invincible);
	}

	bool ut_turret(const unit_t* u) const {
		return ut_flag(u, unit_type_t::flag_turret);
	};
	bool ut_worker(const unit_t* u) const {
		return ut_flag(u, unit_type_t::flag_worker);
	};
	bool ut_hero(const unit_t* u) const {
		return ut_flag(u, unit_type_t::flag_hero);
	};
	bool ut_building(const unit_t* u) const {
		return ut_flag(u, unit_type_t::flag_building);
	};
	bool ut_flyer(const unit_t* u) const {
		return ut_flag(u, unit_type_t::flag_flyer);
	};
	bool ut_can_turn(const unit_t* u) const {
		return ut_flag(u, unit_type_t::flag_can_turn);
	};
	bool ut_can_move(const unit_t* u) const {
		return ut_flag(u, unit_type_t::flag_can_move);
	};
	bool ut_invincible(const unit_t* u) const {
		return ut_flag(u, unit_type_t::flag_invincible);
	};
	bool ut_two_units_in_one_egg(const unit_t* u) const {
		return ut_flag(u, unit_type_t::flag_two_units_in_one_egg);
	};
	bool ut_regens_hp(const unit_t* u) const {
		return ut_flag(u, unit_type_t::flag_regens_hp);
	}
	bool ut_flying_building(const unit_t* u) const {
		return ut_flag(u, unit_type_t::flag_flying_building);
	}
	bool ut_has_energy(const unit_t* u) const {
		return ut_flag(u, unit_type_t::flag_has_energy);
	}
	bool ut_resource(const unit_t* u) const {
		return ut_flag(u, unit_type_t::flag_resource);
	}

	bool us_selected(const unit_t* u) const {
		return us_flag(u, sprite_t::flag_selected);
	}
	bool us_hidden(const unit_t* u) const {
		return us_flag(u, sprite_t::flag_hidden);
	}

	const unit_type_t* get_unit_type(int id) const {
		if ((size_t)id >= 228) xcept("invalid unit id %d", id);
		return &game_st.unit_types.vec[id];
	}
	const image_type_t* get_image_type(int id) const {
		if ((size_t)id >= 999) xcept("invalid image id %d", id);
		return &global_st.image_types.vec[id];
	}

	unit_t* get_unit(unit_id id) const {
		size_t idx = id.index();
		if (!idx) return nullptr;
		size_t actual_index = idx - 1;
		if (actual_index >= st.units.size()) xcept("attempt to dereference invalid unit id %d (actual index %d)", idx, actual_index);
		unit_t*u = &st.units[actual_index];
		if (u->unit_id_generation != id.generation()) return nullptr;
		return u;
	};

	bool is_in_map_bounds(const unit_type_t* unit_type, xy pos) const {
		if (pos.x - unit_type->dimensions.from.x < 0) return false;
		if (pos.y - unit_type->dimensions.from.y < 0) return false;
		if ((size_t)(pos.x + unit_type->dimensions.to.x) >= game_st.map_width) return false;
		if ((size_t)(pos.y + unit_type->dimensions.to.y) >= game_st.map_height) return false;
		return true;
	};
	bool is_in_map_bounds(rect bounds) const {
		if (bounds.from.x < 0) return false;
		if ((size_t)bounds.to.x >= game_st.map_width) return false;
		if (bounds.from.y < 0) return false;
		if ((size_t)bounds.to.y >= game_st.map_height) return false;
		return true;
	}

	rect unit_type_bounding_box(const unit_type_t* unit_type, xy origin = xy()) const {
		return{ origin - unit_type->dimensions.from, origin + unit_type->dimensions.to };
	}
	rect unit_bounding_box(const unit_t* u, xy origin) const {
		return unit_type_bounding_box(u->unit_type, origin);
	}

	rect unit_sprite_bounding_box(const unit_t* u) const {
		return{ u->sprite->position - u->unit_type->dimensions.from, u->sprite->position + u->unit_type->dimensions.to };
	}

	xy restrict_unit_pos_to_map_bounds(xy pos, const unit_type_t* ut) const {
		rect bb = unit_type_bounding_box(ut, pos);
		if (bb.from.x < 0) pos.x -= bb.from.x;
		else if ((size_t)bb.to.x >= game_st.map_width) pos.x -= (size_t)bb.to.x - game_st.map_width + 1;
		if (bb.from.y < 0) pos.y -= bb.from.y;
		else if ((size_t)bb.to.y >= game_st.map_height - 32) pos.y -= (size_t)bb.to.y - game_st.map_height + 32 + 1;
		return pos;
	}

	bool is_walkable(xy pos) const {
		size_t index = tile_index(pos);
		auto& tile = st.tiles[index];
		if (tile.flags & tile_t::flag_has_creep) return true;
		if (tile.flags & tile_t::flag_partially_walkable) {
			size_t ux = pos.x;
			size_t uy = pos.y;
			size_t megatile_index = st.tiles_mega_tile_index[index];
			int flags = game_st.vf4.at(megatile_index).flags[uy / 8 % 4 * 4 + ux / 8 % 4];
			return flags & MiniTileFlags::Walkable;
		}
		if (tile.flags & tile_t::flag_walkable) return true;
		return false;
	}

	void tiles_flags_and(int offset_x, int offset_y, int width, int height, int flags) {
		if ((size_t)(offset_x + width) > game_st.map_tile_width) xcept("attempt to mask tile out of bounds");
		if ((size_t)(offset_y + height) > game_st.map_tile_height) xcept("attempt to mask tile out of bounds");
		for (int y = offset_y; y < offset_y + height; ++y) {
			for (int x = offset_x; x < offset_x + width; ++x) {
				st.tiles[x + y * game_st.map_tile_width].flags &= flags;
			}
		}
	}
	void tiles_flags_or(int offset_x, int offset_y, int width, int height, int flags) {
		if ((size_t)(offset_x + width) > game_st.map_tile_width) xcept("attempt to mask tile out of bounds");
		if ((size_t)(offset_y + height) > game_st.map_tile_height) xcept("attempt to mask tile out of bounds");
		for (int y = offset_y; y < offset_y + height; ++y) {
			for (int x = offset_x; x < offset_x + width; ++x) {
				st.tiles[x + y * game_st.map_tile_width].flags |= flags;
			}
		}
	}

	bool unit_type_spreads_creep(const unit_type_t* ut, bool include_non_evolving) const {
		if (ut->id == UnitTypes::Zerg_Hatchery && include_non_evolving) return true;
		if (ut->id == UnitTypes::Zerg_Lair) return true;
		if (ut->id == UnitTypes::Zerg_Hive) return true;
		if (ut->id == UnitTypes::Zerg_Creep_Colony && include_non_evolving) return true;
		if (ut->id == UnitTypes::Zerg_Spore_Colony) return true;
		if (ut->id == UnitTypes::Zerg_Sunken_Colony) return true;
		return false;
	}

	void update_sprite_some_images_set_redraw(sprite_t* sprite) {
		for (image_t* img : ptr(sprite->images)) {
			if (img->palette_type == 0xb) img->flags |= image_t::flag_redraw;
		}
	};

	int visible_hp_plus_shields(const unit_t* u) const {
		int r = 0;
		if (u->unit_type->has_shield) r += u->shield_points.integer_part();
		r += u->hp.ceil().integer_part();
		return r;
	};
	int max_visible_hp(const unit_t* u) const {
		int hp = u->unit_type->hitpoints.integer_part();
		if (hp == 0) hp = u->hp.ceil().integer_part();
		if (hp == 0) hp = 1;
		return hp;
	};
	int max_visible_hp_plus_shields(const unit_t* u) const {
		int shields = 0;
		if (u->unit_type->has_shield) shields += u->unit_type->shield_points;
		return max_visible_hp(u) + shields;
	};

	size_t unit_space_occupied(const unit_t* u) const {
		size_t r = 0;
		for (auto id : u->loaded_units) {
			unit_t* nu = get_unit(id);
			if (!nu || unit_dead(nu)) continue;
			r += nu->unit_type->space_required;;
		}
		return r;
	}

	int get_unit_strength(unit_t* u, bool ground) const {
		if (u->unit_type->id == UnitTypes::Zerg_Larva || u->unit_type->id == UnitTypes::Zerg_Egg || u->unit_type->id == UnitTypes::Zerg_Cocoon || u->unit_type->id == UnitTypes::Zerg_Lurker_Egg) return 0;
		int vis_hp_shields = visible_hp_plus_shields(u);
		int max_vis_hp_shields = max_visible_hp_plus_shields(u);
		if (u_hallucination(u)) {
			if (vis_hp_shields < max_vis_hp_shields) return 0;
		}

		int r = ground ? game_st.unit_ground_strength[u->unit_type->id] : game_st.unit_air_strength[u->unit_type->id];
		if (u->unit_type->id == UnitTypes::Terran_Bunker) {
			r = ground ? game_st.unit_ground_strength[UnitTypes::Terran_Marine] : game_st.unit_air_strength[UnitTypes::Terran_Marine];
			r *= unit_space_occupied(u);
		}
		if (ut_has_energy(u) && !u_hallucination(u)) {
			r += u->energy.integer_part() / 2;
		}
		return r * vis_hp_shields / max_vis_hp_shields;
	};

	void set_unit_hp(unit_t* u, fp8 hitpoints) {
		u->hp = std::min(hitpoints, u->unit_type->hitpoints);
		if (us_selected(u) && u->sprite->visibility_flags&st.local_mask) {
			update_sprite_some_images_set_redraw(u->sprite);
		}
		if (u_completed(u)) {
			// damage overlay stuff

			u->air_strength = get_unit_strength(u, false);
			u->ground_strength = get_unit_strength(u, true);
		}
	};

	void set_unit_shield_points(unit_t* u, fp8 shield_points) {
		u->shield_points = std::min(shield_points, fp8::integer(u->unit_type->shield_points));
	}

	void set_unit_energy(unit_t* u, fp8 energy) {
		u->energy = std::min(energy, unit_max_energy(u));
	}

	void set_unit_resources(unit_t* u, int resources) {
		if (ut_resource(u)) return;
		u->building.resource.resource_count = resources;
		if (u->unit_type->id >= UnitTypes::Resource_Mineral_Field && u->unit_type->id <= UnitTypes::Resource_Mineral_Field_Type_3) {
			int anim = iscript_anims::WorkingToIdle;
			if (resources < 250) anim = iscript_anims::SpecialState1;
			else if (resources < 500) anim = iscript_anims::SpecialState2;
			else if (resources < 7500) anim = iscript_anims::AlmostBuilt;
			if (u->building.resource.resource_iscript != anim) {
				u->building.resource.resource_iscript = anim;
				sprite_run_anim(u->sprite, anim);
			}
		}
	}

	bool is_frozen(const unit_t* u) const {
		if (u_frozen(u)) return true;
		if (u->lockdown_timer) return true;
		if (u->stasis_timer) return true;
		if (u->maelstrom_timer) return true;
		return false;
	};

	void set_current_button_set(unit_t* u, int type) {
		if (type != UnitTypes::None && !ut_building(u)) {
			if (is_frozen(u)) return;
		}
		u->current_button_set = type;
	};

	image_t* find_image(sprite_t* sprite, int first_id, int last_id) const {
		for (image_t* i : ptr(sprite->images)) {
			if (i->image_type->id >= first_id && i->image_type->id <= last_id) return i;
		}
		return nullptr;
	};

	void freeze_effect_end(unit_t* u, int first, int last) {
		bool still_frozen = is_frozen(u);
		if (u->subunit && !still_frozen) {
			u->status_flags &= ~StatusFlags::DoodadStatesThing;
			xcept("freeze_effect_end: orderComputer_cl");
			// orderComputer_cl(u->subunit, units_dat.ReturntoIdle[u->subunit->unit_type]);
		}
		image_t*image = find_image(u->sprite, first, last);
		if (!image && u->subunit) image = find_image(u->subunit->sprite, first, last);
		if (image) iscript_run_anim(image, iscript_anims::Death);
		if (u->unit_type->flags & UnitPrototypeFlags::Worker && !still_frozen) {
			// sub_468DB0
			unit_t*target = u->worker.harvest_target;
			if (target && target->unit_type->flags & UnitPrototypeFlags::FlyingBuilding) {
				if (u->worker.is_carrying_something) {
					if (target->building.resource.gather_queue_count) {
						//if (u->order_id )
						xcept("weird logic, fix me when this throws");
					}
				}
			}
		}
		u->order_queue_timer = 15;
	};

	void remove_stasis(unit_t* u) {
		u->stasis_timer = 0;
		set_current_button_set(u, u->unit_type->id);
		u_set_status_flag(u, unit_t::status_flag_invincible, ut_invincible(u));
		freeze_effect_end(u, idenums::IMAGEID_Stasis_Field_Small, idenums::IMAGEID_Stasis_Field_Large);
	};

	void update_unit_status_timers(unit_t* u) {
		if (u->stasis_timer) {
			--u->stasis_timer;
			if (!u->stasis_timer) {
				remove_stasis(u);
			}
		}
		if (u->stim_timer) {
			--u->stim_timer;
			if (!u->stim_timer) {
				xcept("remove stim");
			}
		}
		if (u->ensnare_timer) {
			--u->ensnare_timer;
			if (!u->ensnare_timer) {
				xcept("remove ensnare");
			}
		}
		if (u->defense_matrix_timer) {
			--u->defense_matrix_timer;
			if (!u->defense_matrix_timer) {
				xcept("remove defense matrix");
			}
		}
		if (u->irradiate_timer) {
			--u->irradiate_timer;
			xcept("irradiate damage");
			if (!u->irradiate_timer) {
				xcept("remove irradiate");
			}
		}
		if (u->lockdown_timer) {
			--u->lockdown_timer;
			if (!u->lockdown_timer) {
				xcept("remove lockdown");
			}
		}
		if (u->maelstrom_timer) {
			--u->maelstrom_timer;
			if (!u->maelstrom_timer) {
				xcept("remove maelstrom");
			}
		}
		if (u->plague_timer) {
			xcept("plague stuff");
		}
		if (u->storm_timer) --u->storm_timer;
		int prev_acidSporeCount = u->acid_spore_count;
		for (auto&v : u->acid_spore_time) {
			if (!v) continue;
			--v;
			if (!v) --u->acid_spore_count;
		}
		if (u->acid_spore_count) {
			xcept("acid spore stuff");
		} else if (prev_acidSporeCount) {
			xcept("RemoveOverlays(u, IMAGEID_Acid_Spores_1_Overlay_Small, IMAGEID_Acid_Spores_6_9_Overlay_Large);");
		}

	};

	bool create_selection_circle(sprite_t* sprite, int color, int imageid) {
		return false;
	};

	void remove_selection_circle(sprite_t* sprite) {

	};

	void update_selection_sprite(sprite_t* sprite, int color) {
		if (!sprite->selection_timer) return;
		--sprite->selection_timer;
		if (~sprite->visibility_flags&st.local_mask) sprite->selection_timer = 0;
		if (sprite->selection_timer & 8 || (sprite->selection_timer == 0 && sprite->flags&SpriteFlags::Selected)) {
			if (~sprite->flags&SpriteFlags::DrawSelection) {
				if (create_selection_circle(sprite, color, idenums::IMAGEID_Selection_Circle_22pixels)) {
					sprite->flags |= SpriteFlags::DrawSelection;
				}
			}
		} else remove_selection_circle(sprite);
	};

	fp8 unit_cloak_energy_cost(const unit_t* u) const {
		switch (u->unit_type->id) {
		case UnitTypes::Terran_Ghost:
		case UnitTypes::Hero_Sarah_Kerrigan:
		case UnitTypes::Hero_Alexei_Stukov:
		case UnitTypes::Hero_Samir_Duran:
		case UnitTypes::Hero_Infested_Duran:
		case UnitTypes::Hero_Infested_Kerrigan:
			return fp8::integer(10) / 256;
		case UnitTypes::Terran_Wraith:
		case UnitTypes::Hero_Tom_Kazansky:
			return fp8::integer(13) / 256;
		}
		return fp8::zero();
	}

	void set_secondary_order(unit_t* u, const order_type_t* order_type) {
		if (u->secondary_order_type == order_type) return;
		u->secondary_order_type = order_type;
		u->secondary_order_state = 0;
		u->secondary_order_unk_a = 0;
		u->secondary_order_unk_b = 0;
		u->current_build_unit = nullptr;
	}

	void update_unit_energy(unit_t* u) {
		if (!ut_has_energy(u)) return;
		if (u_hallucination(u)) return;
		if (!u_completed(u)) return;
		if (u_cloaked(u) || u_requires_detector(u)) {
			fp8 cost = unit_cloak_energy_cost(u);
			if (u->energy < cost) {
				if (u->secondary_order_type->id == Orders::Cloak) set_secondary_order(u, get_order_type(Orders::Nothing));
			} else {
				u->energy -= cost;
				if (us_selected(u)) {
					update_sprite_some_images_set_redraw(u->sprite);
				}
			}
		} else {
			fp8 max_energy = unit_max_energy(u);
			if (u->unit_type->id == UnitTypes::Protoss_Dark_Archon && u->order_type->id == Orders::CompletingArchonSummon && u->order_state) {
				max_energy = fp8::integer(50);
			}
			u->energy = std::min(u->energy + fp8::integer(8) / 256, max_energy);
			if (us_selected(u)) {
				update_sprite_some_images_set_redraw(u->sprite);
			}
		}
	};

	int unit_hp_percent(const unit_t* u) const {
		int max_hp = max_visible_hp(u);
		int hp = u->hp.ceil().integer_part();
		return hp * 100 / max_hp;
	};

	void update_unit_values(unit_t* u) {
		if (u->main_order_timer) --u->main_order_timer;
		if (u->ground_weapon_cooldown) --u->ground_weapon_cooldown;
		if (u->air_weapon_cooldown) --u->air_weapon_cooldown;
		if (u->spell_cooldown) --u->spell_cooldown;
		if (u->unit_type->has_shield) {
			fp8 max_shields = fp8::integer(u->unit_type->shield_points);
			if (u->shield_points != max_shields) {
				u->shield_points += fp8::integer(7) / 256;
				if (u->shield_points > max_shields) u->shield_points = max_shields;
				if (us_selected(u)) {
					update_sprite_some_images_set_redraw(u->sprite);
				}
			}
		}
		if (u->unit_type->id == UnitTypes::Zerg_Zergling || u->unit_type->id == UnitTypes::Hero_Devouring_One) {
			if (u->ground_weapon_cooldown == 0) u->order_queue_timer = 0;
		}
		u->is_being_healed = false;
		if (u_completed(u) || ~u->sprite->flags & sprite_t::flag_hidden) {
			++u->cycle_counter;
			if (u->cycle_counter >= 8) {
				u->cycle_counter = 0;
				update_unit_status_timers(u);
			}
		}
		if (u_completed(u)) {
			if (ut_regens_hp(u)) {
				if (u->hp > fp8::zero() && u->hp != u->unit_type->hitpoints) {
					set_unit_hp(u, u->hp + fp8::integer(4) / 256);
				}
			}
			update_unit_energy(u);
			if (u->recent_order_timer) --u->recent_order_timer;
			bool remove_timer_hit_zero = false;
			if (u->remove_timer) {
				--u->remove_timer;
				if (!u->remove_timer) {
					xcept("orders_SelfDestructing...");
					return;
				}
			}
			int gf = u->unit_type->staredit_group_flags;
			if (gf&GroupFlags::Terran && ~gf&(GroupFlags::Zerg | GroupFlags::Protoss)) {
				if (u_grounded_building(u) || ut_flying_building(u)) {
					if (unit_hp_percent(u) <= 33) {
						xcept("killTargetUnitCheck(...)");
					}
				}
			}
		}
	}

	unit_t* unit_turret(const unit_t* u) const {
		if (!u->subunit) return nullptr;
		if (!ut_turret(u->subunit)) return nullptr;
		return u->subunit;
	}

	const unit_t* unit_attacking_unit(const unit_t* u) const {
		return u->subunit && ut_turret(u->subunit) ? u->subunit : u;
	}

	const weapon_type_t* unit_ground_weapon(const unit_t* u) const {
		if (u->unit_type->id == UnitTypes::Zerg_Lurker && !u_burrowed(u)) return nullptr;
		return u->unit_type->ground_weapon;
	}

	const weapon_type_t* unit_air_weapon(const unit_t* u) const {
		return u->unit_type->air_weapon;
	}

	const weapon_type_t* unit_or_subunit_ground_weapon(const unit_t* u) const {
		auto* w = unit_ground_weapon(u);
		if (w || !u->subunit) return w;
		return unit_ground_weapon(u->subunit);
	}

	const weapon_type_t* unit_or_subunit_air_weapon(const unit_t* u) const {
		auto* w = unit_air_weapon(u);
		if (w || !u->subunit) return w;
		return unit_air_weapon(u->subunit);
	}

	const weapon_type_t* unit_target_weapon(const unit_t* u, const unit_t* target) const {
		return u_flying(target) ? unit_air_weapon(unit_attacking_unit(u)) : unit_ground_weapon(unit_attacking_unit(u));
	}

	bool unit_is_carrier(const unit_t* u) const {
		return u->unit_type->id == UnitTypes::Protoss_Carrier || u->unit_type->id == UnitTypes::Hero_Gantrithor;
	}

	bool unit_is_reaver(const unit_t* u) const {
		return u->unit_type->id == UnitTypes::Protoss_Reaver || u->unit_type->id == UnitTypes::Hero_Warbringer;
	}

	bool unit_is_queen(const unit_t* u) const {
		return u->unit_type->id == UnitTypes::Zerg_Queen || u->unit_type->id == UnitTypes::Hero_Matriarch;
	}

	bool unit_target_is_undetected(const unit_t* u, const unit_t* target) const {
		if (!u_cloaked(target) && !u_requires_detector(target)) return false;
		if (u->visibility_flags & (1 << u->owner)) return false;
		return true;
	}

	bool unit_target_is_visible(const unit_t* u, const unit_t* target) const {
		if (target->sprite->visibility_flags & (1 << u->owner)) return true;
		return true;
	}

	bool is_reachable(xy from, xy to) const {
		return st.paths.get_region_at(from)->group_index == st.paths.get_region_at(to)->group_index;
	}

	bool cc_can_be_infested(const unit_t* u) const {
		if (u->unit_type->id != UnitTypes::Terran_Command_Center) return false;
		if (!u_completed(u)) return false;
		return unit_hp_percent(u) < 50;
	}

	bool unit_can_attack_target(const unit_t* u, const unit_t* target) const {
		if (!target) return false;
		if (is_frozen(target)) return false;
		if (u_invincible(u)) return false;
		if (ut_invincible(u)) return false;
		if (us_hidden(u)) return false;
		if (unit_target_is_undetected(u, target)) return false;
		if (unit_is_carrier(u)) return true;
		if (unit_is_reaver(u)) {
			if (u_flying(target)) return false;
			return is_reachable(u->sprite->position, target->sprite->position);
		}
		if (unit_is_queen(u)) {
			return cc_can_be_infested(target);
		}
		return unit_target_weapon(u, target) != nullptr;
	}

	bool unit_autoattack(unit_t* u) {
		if (!u->auto_target_unit) return false;
		if (unit_target_is_enemy(u, u->auto_target_unit)) {
			if (unit_can_attack_target(u, u->auto_target_unit)) {
				xcept("auto attack waa");
				return true;
			}
		} else {
			u->auto_target_unit = nullptr;
		}
		return false;
	}

	xy rect_difference(rect a, rect b) const {
		int x, y;
		if (a.from.x > b.to.x) x = a.from.x - b.to.x;
		else if (b.from.x > a.to.x) x = b.from.x - a.to.x;
		else x = 0;
		if (a.from.y > b.to.y) y = a.from.y - b.to.y;
		else if (b.from.y > a.to.y) y = b.from.y - a.to.y;
		else y = 0;

		return xy(x, y);
	}

	int xy_length(xy vec) const {
		unsigned int x = std::abs(vec.x);
		unsigned int y = std::abs(vec.y);
		if (x < y) std::swap(x, y);
		if (x / 4 < y) x = x - x / 16 + (y * 3) / 8 - x / 64 + (y * 3) / 256;
		return x;
	}

	int units_distance(const unit_t* a, const unit_t* b) const {
		auto a_rect = unit_sprite_bounding_box(a);
		auto b_rect = unit_sprite_bounding_box(b);
		b_rect.to += {1, 1};
		return xy_length(rect_difference(a_rect, b_rect));
	}

	// atan is done with an inverse lookup using binary search.
	template<size_t integer_bits>
	direction_t sc_atan(fixed_point<integer_bits, 8, true> x) const {
		bool negative = x < decltype(x)::zero();
		if (negative) x = -x;
		int r;
		if ((unsigned int)x.raw_value > std::numeric_limits<unsigned int>::max()) r = 63;
		else r = std::upper_bound(arctan_inv_table.begin(), arctan_inv_table.end(), (unsigned int)x.raw_value) - arctan_inv_table.begin();
		return negative ? -direction_t::from_raw(r) : direction_t::from_raw(r);
	};

	direction_t xy_direction(xy_fp8 pos) const {
		if (pos.x == fp8::zero()) return pos.y <= fp8::zero() ? direction_t::zero() : direction_t::from_raw(-128);
		direction_t r = sc_atan(pos.y / pos.x);
		if (pos.x > fp8::zero()) r += direction_t::from_raw(64);
		else r = -r;
		return r;
	}

	direction_t xy_direction(xy pos) const {
		if (pos.x == 0) return pos.y <= 0 ? direction_t::zero() : direction_t::from_raw(-128);
		direction_t r = sc_atan(fp8::integer(pos.y) / pos.x);
		if (pos.x > 0) r += direction_t::from_raw(64);
		else r = -r;
		return r;
	}

	xy_fp8 direction_xy(direction_t dir, fp8 length) {
		return global_st.direction_table[direction_index(dir)] * length;
	}

	size_t direction_index(direction_t dir) {
		auto v = dir.fractional_part();
		if (v < 0) return 256 + v;
		else return v;
	}

	direction_t units_direction(const unit_t* from, const unit_t* to) const {
		xy relpos = to->sprite->position - from->sprite->position;
		return xy_direction(relpos);
	}

	bool unit_target_in_attack_angle(const unit_t* u, const unit_t* target, const weapon_type_t* weapon) const {
		auto dir = units_direction(u, target);
		if (u->unit_type->id == UnitTypes::Zerg_Lurker) {
			xcept("unit_target_in_attack_angle lurker: fixme");
			// For some reason, this field is set here for lurkers, but I would really like u to be const.
			// todo: figure out if it is necessary.
			//u->current_direction1 = dir;
			return true;
		}
		return (dir - u->heading).abs() <= weapon->attack_angle;
	}

	int weapon_max_range(const unit_t* u, const weapon_type_t* w) const {
		int bonus = 0;
		auto has_upgrade = [&](int id) {
			return st.upgrade_levels[u->owner][id];
		};
		auto range_upgrade_bonus = [&]() {
			switch (u->unit_type->id) {
			case UnitTypes::Terran_Marine:
				return has_upgrade(UpgradeTypes::U_238_Shells) ? 32 : 0;
			case UnitTypes::Zerg_Hydralisk:
				return has_upgrade(UpgradeTypes::Grooved_Spines) ? 32 : 0;
			case UnitTypes::Protoss_Dragoon:
				return has_upgrade(UpgradeTypes::Singularity_Charge) ? 64 : 0;
			case UnitTypes::Hero_Fenix_Dragoon:
				return 64;
			case UnitTypes::Terran_Goliath:
			case UnitTypes::Terran_Goliath_Turret:
				return w->id == WeaponTypes::Hellfire_Missile_Pack && has_upgrade(UpgradeTypes::Charon_Boosters) ? 96 : 0;
			case UnitTypes::Hero_Alan_Schezar:
			case UnitTypes::Hero_Alan_Schezar_Turret:
				return w->id == WeaponTypes::Hellfire_Missile_Pack_Alan_Schezar ? 96 : 0;
			};
			return 0;
		};
		int r = 0;
		if (u_in_building(u)) r += 64;
		r += range_upgrade_bonus();
		return r;
	}

	int unit_target_movement_range(const unit_t* u, const unit_t* target) const {
		if (!u_movement_flag(u, 2)) return 0;
		if (u_movement_flag(target, 2)) {
			if ((target->velocity_direction - u->velocity_direction).abs() <= direction_t::from_raw(32)) return 0;
		}
		return unit_halt_distance(u).integer_part();
	}

	bool unit_target_in_weapon_movement_range(const unit_t* u, const unit_t* target) const {
		if (!target) target = u->order_target.unit;
		if (!target) return true;
		if (!unit_target_is_visible(u, target)) return false;
		auto* w = unit_target_weapon(u, target);
		if (!w) return false;
		int d = units_distance(u, target);
		if (w->min_range && d < w->min_range) return false;
		int max_range = weapon_max_range(u, w);
		max_range += unit_target_movement_range(u, target);
		return d <= max_range;
	}

	bool some_unit_target_computer_thing(const unit_t* u, const unit_t* target) const {
		if (st.players[u->owner].controller != state::player_t::controller_computer_game) return false;
		if (u_flying(u)) return false;
		if (unit_target_in_weapon_movement_range(u, target)) return false;
		return u_status_flag(u, (unit_t::status_flags_t)0x80);
	}

	unit_t* unit_first_loaded_unit(const unit_t* u) const {
		for (size_t i = 0; i != u->unit_type->space_provided; ++i) {
			unit_t* nu = get_unit(u->loaded_units.at(i));
			if (!nu || unit_dead(nu)) continue;
			return nu;
		}
		return nullptr;
	}

	size_t unit_interceptor_count(const unit_t* u) const {
		if (!unit_is_carrier(u)) return 0;
		return u->carrier.inside_count + u->carrier.outside_count;
	}
	size_t unit_scarab_count(const unit_t* u) const {
		if (!unit_is_reaver(u)) return 0;
		return u->reaver.inside_count + u->reaver.outside_count;
	}

	bool unit_can_attack(const unit_t* u) const {
		if (unit_or_subunit_ground_weapon(u) || unit_or_subunit_air_weapon(u)) return true;
		if (unit_interceptor_count(u)) return true;
		if (unit_scarab_count(u)) return true;
		return false;
	}

	int unit_target_attack_priority(const unit_t* u, const unit_t* target) const {
		bool is_loaded_unit = false;
		if (target->unit_type->id == UnitTypes::Terran_Bunker) {
			const unit_t* loaded_unit = unit_first_loaded_unit(u);
			if (loaded_unit) {
				target = loaded_unit;
				is_loaded_unit = true;
			}
		}
		if (target->unit_type->id == UnitTypes::Zerg_Larva) return 5;
		if (target->unit_type->id == UnitTypes::Zerg_Egg) return 5;
		if (target->unit_type->id == UnitTypes::Zerg_Cocoon) return 5;
		if (target->unit_type->id == UnitTypes::Zerg_Lurker_Egg) return 5;
		int r = 0;
		if (ut_worker(u)) r += 2;
		else if (!unit_can_attack_target(u, target)) {
			if (unit_can_attack(target)) r += 2;
			else if (u_can_move(u)) r += 3;
			else r += 4;
		}
		if (is_loaded_unit || !u_completed(u)) ++r;
		if (r == 0 && u_cannot_attack(u)) ++r;
		return r;
	}

	auto get_default_priority_targets(const unit_t* u, int min_distance, int max_distance) const {
		xy pos = u->sprite->position;
		rect bounds { { pos.x - max_distance - 64, pos.y - max_distance - 64 }, { pos.x + max_distance + 64, pos.y + max_distance + 64 } };
		const unit_t* attacking_unit = unit_attacking_unit(u);
		bool can_turn = u_can_turn(attacking_unit);
		std::array<static_vector<unit_t*, 0x10>, 6> targets {};
		for (unit_t* target : find_units(bounds)) {
			if (target == u) continue;
			if (!unit_target_is_enemy(u, target)) continue;
			if (!unit_target_is_visible(u, target)) continue;
			if (!unit_can_attack_target(u, target)) continue;
			int distance = units_distance(u, target);
			if (min_distance && distance < min_distance) continue;
			if (distance > max_distance) continue;
			if (!can_turn) {
				if (!attacking_unit->unit_type->ground_weapon) xcept("find_acquire_target: null ground weapon (unreachable?)");
				if (!unit_target_in_attack_angle(attacking_unit, target, attacking_unit->unit_type->ground_weapon)) continue;
				if (!some_unit_target_computer_thing(u, target)) {
					int prio = unit_target_attack_priority(u, target);
					if (targets[prio].size() < 0x10) targets[prio].push_back(target);
				}
			}
		}
		return targets;
	}

	unit_t* find_acquire_target(const unit_t* u) const {
		int acq_range = unit_target_acquisition_range(u);
		if (u_in_building(u)) acq_range += 2;

		int min_distance = 0;
		int max_distance = acq_range * 32;

		auto* ground_weapon = unit_ground_weapon(u);
		if (!ground_weapon && u->subunit) ground_weapon = unit_ground_weapon(u->subunit);
		auto* air_weapon = unit_air_weapon(u);
		if (!air_weapon && u->subunit) air_weapon = unit_air_weapon(u->subunit);
		if (ground_weapon) {
			if (!air_weapon) min_distance = ground_weapon->min_range;
			else min_distance = std::min(ground_weapon->min_range, air_weapon->min_range);
		} else {
			if (air_weapon) min_distance = air_weapon->min_range;
		}
		auto targets = get_default_priority_targets(u, min_distance, max_distance);
		for (auto& v : targets) {
			if (v.empty()) continue;
			return get_best_score(v, [&](const unit_t* target) {
				return xy_length(target->sprite->position - u->sprite->position);
			});
		}
		return nullptr;
	}

	void order_destroy(unit_t* u) {
		xcept("order destroy %p\n", u);
	}

	void order_Guard(unit_t* u) {
		u->main_order_timer = lcg_rand(29, 0, 15);
		u->order_type = get_order_type(Orders::PlayerGuard);
	}

	void order_PlayerGuard(unit_t* u) {
		if (!unit_autoattack(u)) {
			if (u->main_order_timer == 0) {
				u->main_order_timer = 15;
				if (ut_turret(u)) {
					if (u->next_target_waypoint != u->subunit->next_target_waypoint) {
						u->next_target_waypoint = u->subunit->next_target_waypoint;
					}
				}
				if (unit_target_acquisition_range(u)) {
					unit_t* target = find_acquire_target(u);
					if (target) xcept("waa set target!");
				}
			}
		}
	}

	void order_TurretGuard(unit_t* u) {
		if (u->next_target_waypoint != u->subunit->next_target_waypoint) {
			u->next_target_waypoint = u->subunit->next_target_waypoint;
		}
		order_PlayerGuard(u);
	}

	bool execute_main_order(unit_t* u) {

		switch (u->order_type->id) {
		case Orders::Die:
			xcept("Die");
			return false;
		case Orders::IncompleteWarping:
			xcept("IncompleteWarping");
			return true;
		case Orders::NukeTrack:
			xcept("NukeTrack");
			return true;
		case Orders::WarpIn:
			xcept("WarpIn");
			return true;
		}

		if (is_frozen(u) || (!u_can_move(u) && u_cannot_attack(u))) {
			if (u->main_order_timer == 0) u->main_order_timer = 15;
			if (is_frozen(u)) return true;
		}

		switch (u->order_type->id) {
		case Orders::TurretGuard:
			order_TurretGuard(u);
			break;
		case Orders::TurretAttack:
			xcept("TurretAttack");
			break;
		case Orders::DroneBuild:
			xcept("DroneBuild");
			break;
		case Orders::PlaceBuilding:
			xcept("PlaceBuilding");
			break;
		case Orders::PlaceProtossBuilding:
			xcept("PlaceProtossBuilding");
			break;
		case Orders::ConstructingBuilding:
			xcept("ConstructingBuilding");
			break;
		case Orders::Repair:
			xcept("Repair");
			break;
		case Orders::ZergBirth:
			xcept("ZergBirth");
			break;
		case Orders::ZergUnitMorph:
			xcept("ZergUnitMorph");
			break;
		case Orders::IncompleteBuilding:
			xcept("IncompleteBuilding");
			break;
		case Orders::IncompleteMorphing:
			xcept("IncompleteMorphing");
			break;
		case Orders::ScarabAttack:
			xcept("ScarabAttack");
			break;
		case Orders::RechargeShieldsUnit:
			xcept("RechargeShieldsUnit");
			break;
		case Orders::BuildingLand:
			xcept("BuildingLand");
			break;
		case Orders::BuildingLiftOff:
			xcept("BuildingLiftOff");
			break;
		case Orders::ResearchTech:
			xcept("ResearchTech");
			break;
		case Orders::Upgrade:
			xcept("Upgrade");
			break;
		case Orders::Harvest3:
			xcept("Harvest3");
			break;
		case Orders::Harvest4:
			xcept("Harvest4");
			break;
		case Orders::Interrupted:
			xcept("Interrupted");
			break;
		case Orders::Sieging:
			xcept("Siegeing");
			break;
		case Orders::Unsieging:
			xcept("Unsiegeing");
			break;
		case Orders::ArchonWarp:
			xcept("ArchonWarp");
			break;
		case Orders::CompletingArchonSummon:
			xcept("CompletingArchonSummon");
			break;
		case Orders::NukeTrain:
			xcept("NukeTrain");
			break;
		case Orders::InitializeArbiter:
			xcept("InitializeArbiter");
			break;
		case Orders::ResetCollision:
			xcept("ResetCollision");
			break;
		case Orders::ResetHarvestCollision:
			xcept("ResetHarvestCollision");
			break;
		case Orders::CTFCOP2:
			xcept("CTFCOP2");
			break;
		case Orders::SelfDestructing:
			xcept("SelfDestructing");
			break;
		case Orders::Critter:
			xcept("Critter");
			break;
		case Orders::MedicHeal:
			xcept("MedicHeal");
			break;
		case Orders::HealMove:
			xcept("HealMove");
			break;
		case Orders::MedicHoldPosition:
			xcept("MedicHoldPosition");
			break;
		case Orders::MedicHealToIdle:
			xcept("MedicHealToIdle");
			break;
		case Orders::DarkArchonMeld:
			xcept("DarkArchonMeld");
			break;
		}
		if (u->order_queue_timer) {
			--u->order_queue_timer;
			return true;
		}
		u->order_queue_timer = 8;
		switch (u->order_type->id) {
		case Orders::Die:
			xcept("Die");
			break;
		case Orders::Stop:
			xcept("Stop");
			break;
		case Orders::Guard:
			order_Guard(u);
			break;
		case Orders::PlayerGuard:
			order_PlayerGuard(u);
			break;
		case Orders::BunkerGuard:
			xcept("BunkerGuard");
			break;
		case Orders::Move:
			xcept("Move");
			break;
		case Orders::Attack1:
			xcept("Attack1");
			break;
		case Orders::Attack2:
			xcept("Attack2");
			break;
		case Orders::AttackUnit:
			xcept("AttackUnit");
			break;
		case Orders::Hover:
			xcept("Hover");
			break;
		case Orders::AttackMove:
			xcept("AttackMove");
			break;
		case Orders::UnusedNothing:
			xcept("UnusedNothing");
			break;
		case Orders::UnusedPowerup:
			xcept("UnusedPowerup");
			break;
		case Orders::TowerGuard:
			xcept("TowerGuard");
			break;
		case Orders::TowerAttack:
			xcept("TowerAttack");
			break;
		case Orders::VultureMine:
			xcept("VultureMine");
			break;
		case Orders::TurretAttack:
			xcept("TurretAttack");
			break;
		case Orders::Unused_24:
			xcept("Unused_24");
			break;
		case Orders::DroneBuild:
			xcept("DroneBuild");
			break;
		case Orders::CastInfestation:
			xcept("CastInfestation");
			break;
		case Orders::MoveToInfest:
			xcept("MoveToInfest");
			break;
		case Orders::PlaceProtossBuilding:
			xcept("PlaceProtossBuilding");
			break;
		case Orders::Repair:
			xcept("Repair");
			break;
		case Orders::MoveToRepair:
			xcept("MoveToRepair");
			break;
		case Orders::ZergUnitMorph:
			xcept("ZergUnitMorph");
			break;
		case Orders::IncompleteMorphing:
			xcept("IncompleteMorphing");
			break;
		case Orders::BuildNydusExit:
			xcept("BuildNydusExit");
			break;
		case Orders::IncompleteWarping:
			xcept("IncompleteWarping");
			break;
		case Orders::Follow:
			xcept("Follow");
			break;
		case Orders::Carrier:
			xcept("Carrier");
			break;
		case Orders::ReaverCarrierMove:
			xcept("ReaverCarrierMove");
			break;
		case Orders::CarrierStop:
			xcept("CarrierStop");
			break;
		case Orders::CarrierAttack:
			xcept("CarrierAttack");
			break;
		case Orders::CarrierMoveToAttack:
			xcept("CarrierMoveToAttack");
			break;
		case Orders::CarrierIgnore2:
			xcept("CarrierIgnore2");
			break;
		case Orders::CarrierFight:
			xcept("CarrierFight");
			break;
		case Orders::CarrierHoldPosition:
			xcept("CarrierHoldPosition");
			break;
		case Orders::Reaver:
			xcept("Reaver");
			break;
		case Orders::ReaverAttack:
			xcept("ReaverAttack");
			break;
		case Orders::ReaverMoveToAttack:
			xcept("ReaverMoveToAttack");
			break;
		case Orders::ReaverFight:
			xcept("ReaverFight");
			break;
		case Orders::TrainFighter:
			xcept("TrainFighter");
			break;
		case Orders::RechargeShieldsUnit:
			xcept("RechargeShieldsUnit");
			break;
		case Orders::ShieldBattery:
			xcept("ShieldBattery");
			break;
		case Orders::InterceptorReturn:
			xcept("InterceptorReturn");
			break;
		case Orders::DroneLiftOff:
			xcept("DroneLiftOff");
			break;
		case Orders::Upgrade:
			xcept("Upgrade");
			break;
		case Orders::SpawningLarva:
			xcept("SpawningLarva");
			break;
		case Orders::Harvest1:
			xcept("Harvest1");
			break;
		case Orders::Harvest2:
			xcept("Harvest2");
			break;
		case Orders::MoveToGas:
			xcept("MoveToGas");
			break;
		case Orders::WaitForGas:
			xcept("WaitForGas");
			break;
		case Orders::HarvestGas:
			xcept("HarvestGas");
			break;
		case Orders::ReturnGas:
			xcept("ReturnGas");
			break;
		case Orders::MoveToMinerals:
			xcept("MoveToMinerals");
			break;
		case Orders::WaitForMinerals:
			xcept("WaitForMinerals");
			break;
		case Orders::Harvest4:
			xcept("Harvest4");
			break;
		case Orders::Interrupted:
			xcept("Interrupted");
			break;
		case Orders::EnterTransport:
			xcept("EnterTransport");
			break;
		case Orders::PickupIdle:
			xcept("PickupIdle");
			break;
		case Orders::PickupTransport:
			xcept("PickupTransport");
			break;
		case Orders::PickupBunker:
			xcept("PickupBunker");
			break;
		case Orders::Pickup4:
			xcept("Pickup4");
			break;
		case Orders::Unsieging:
			xcept("Unsieging");
			break;
		case Orders::WatchTarget:
			xcept("WatchTarget");
			break;
		case Orders::SpreadCreep:
			xcept("SpreadCreep");
			break;
		case Orders::CompletingArchonSummon:
			xcept("CompletingArchonSummon");
			break;
		case Orders::HoldPosition:
			xcept("HoldPosition");
			break;
		case Orders::Decloak:
			xcept("Decloak");
			break;
		case Orders::Unload:
			xcept("Unload");
			break;
		case Orders::MoveUnload:
			xcept("MoveUnload");
			break;
		case Orders::FireYamatoGun:
			xcept("FireYamatoGun");
			break;
		case Orders::MoveToFireYamatoGun:
			xcept("MoveToFireYamatoGun");
			break;
		case Orders::CastLockdown:
			xcept("CastLockdown");
			break;
		case Orders::Burrowing:
			xcept("Burrowing");
			break;
		case Orders::Burrowed:
			xcept("Burrowed");
			break;
		case Orders::Unburrowing:
			xcept("Unburrowing");
			break;
		case Orders::CastDarkSwarm:
			xcept("CastDarkSwarm");
			break;
		case Orders::CastParasite:
			xcept("CastParasite");
			break;
		case Orders::CastSpawnBroodlings:
			xcept("CastSpawnBroodlings");
			break;
		case Orders::NukeTrain:
			xcept("NukeTrain");
			break;
		case Orders::NukeLaunch:
			xcept("NukeLaunch");
			break;
		case Orders::NukePaint:
			xcept("NukePaint");
			break;
		case Orders::NukeUnit:
			xcept("NukeUnit");
			break;
		case Orders::CloakNearbyUnits:
			xcept("CloakNearbyUnits");
			break;
		case Orders::PlaceMine:
			xcept("PlaceMine");
			break;
		case Orders::RightClickAction:
			xcept("RightClickAction");
			break;
		case Orders::SuicideUnit:
			xcept("SuicideUnit");
			break;
		case Orders::SuicideLocation:
			xcept("SuicideLocation");
			break;
		case Orders::SuicideHoldPosition:
			xcept("SuicideHoldPosition");
			break;
		case Orders::Teleport:
			xcept("Teleport");
			break;
		case Orders::CastScannerSweep:
			xcept("CastScannerSweep");
			break;
		case Orders::Scanner:
			xcept("Scanner");
			break;
		case Orders::CastDefensiveMatrix:
			xcept("CastDefensiveMatrix");
			break;
		case Orders::CastPsionicStorm:
			xcept("CastPsionicStorm");
			break;
		case Orders::CastIrradiate:
			xcept("CastIrradiate");
			break;
		case Orders::CastPlague:
			xcept("CastPlague");
			break;
		case Orders::CastConsume:
			xcept("CastConsume");
			break;
		case Orders::CastEnsnare:
			xcept("CastEnsnare");
			break;
		case Orders::CastStasisField:
			xcept("CastStasisField");
			break;
		case Orders::ResetHarvestCollision:
			xcept("ResetHarvestCollision");
			break;
		case Orders::Patrol:
			xcept("Patrol");
			break;
		case Orders::CTFCOPInit:
			xcept("CTFCOPInit");
			break;
		case Orders::CTFCOP2:
			xcept("CTFCOP2");
			break;
		case Orders::ComputerAI:
			xcept("ComputerAI");
			break;
		case Orders::AtkMoveEP:
			xcept("AtkMoveEP");
			break;
		case Orders::HarassMove:
			xcept("HarassMove");
			break;
		case Orders::AIPatrol:
			xcept("AIPatrol");
			break;
		case Orders::GuardPost:
			xcept("GuardPost");
			break;
		case Orders::RescuePassive:
			xcept("RescuePassive");
			break;
		case Orders::Neutral:
			xcept("Neutral");
			break;
		case Orders::ComputerReturn:
			xcept("ComputerReturn");
			break;
		case Orders::Critter:
			xcept("Critter");
			break;
		case Orders::HiddenGun:
			xcept("HiddenGun");
			break;
		case Orders::OpenDoor:
			xcept("OpenDoor");
			break;
		case Orders::CloseDoor:
			xcept("CloseDoor");
			break;
		case Orders::HideTrap:
			xcept("HideTrap");
			break;
		case Orders::RevealTrap:
			xcept("RevealTrap");
			break;
		case Orders::EnableDoodad:
			xcept("EnableDoodad");
			break;
		case Orders::WarpIn:
			xcept("WarpIn");
			break;
		case Orders::MedicHealToIdle:
			xcept("MedicHealToIdle");
			break;
		case Orders::CastRestoration:
			xcept("CastRestoration");
			break;
		case Orders::CastDisruptionWeb:
			xcept("CastDisruptionWeb");
			break;
		case Orders::DarkArchonMeld:
			xcept("DarkArchonMeld");
			break;
		case Orders::CastFeedback:
			xcept("CastFeedback");
			break;
		case Orders::CastOpticalFlare:
			xcept("CastOpticalFlare");
			break;
		case Orders::CastMaelstrom:
			xcept("CastMaelstrom");
			break;
		}

		return true;
	}

	void execute_secondary_order(unit_t* u) {
		if (u->secondary_order_type->id == Orders::Hallucination2) {
			if (u->defense_matrix_damage || u->stim_timer || u->ensnare_timer || u->lockdown_timer || u->irradiate_timer || u->stasis_timer || u->parasite_flags || u->storm_timer || u->plague_timer || u->is_blind || u->maelstrom_timer) {
				order_destroy(u);
			}
			return;
		}
		if (is_frozen(u)) return;
		switch (u->secondary_order_type->id) {
		case Orders::Train:
			xcept("Train");
			break;
		case Orders::BuildAddon:
			xcept("BuildAddon");
			break;
		case Orders::TrainFighter:
			xcept("TrainFighter");
			break;
		case Orders::ShieldBattery:
			xcept("ShieldBattery");
			break;
		case Orders::SpawningLarva:
			xcept("SpawningLarva");
			break;
		case Orders::SpreadCreep:
			xcept("SpreadCreep");
			break;
		case Orders::Cloak:
			xcept("Cloak");
			break;
		case Orders::Decloak:
			xcept("Decloak");
			break;
		case Orders::CloakNearbyUnits:
			xcept("CloakNearbyUnits");
			break;
		}
	}

	void update_unit(unit_t* u) {
		if (!ut_turret(u) && !us_hidden(u)) {
			update_selection_sprite(u->sprite, st.selection_circle_color[u->owner]);
		}

		update_unit_values(u);

		if (!execute_main_order(u)) return;
		execute_secondary_order(u);

		if (u->subunit && !ut_turret(u)) {
			iscript_unit_setter ius(this, u->subunit);
			update_unit(u->subunit);
		}

		if (u->sprite) {
			if (!iscript_execute_sprite(u->sprite)) u->sprite = nullptr;
		}

		if (!u->sprite) xcept("unit has null sprite");

	};

	int unit_movepos_state(unit_t* u) const {
		if (u->sprite->position != u->move_target.pos) return 0;
		return u_immovable(u) ? 2 : 1;
	};

	bool unit_dead(unit_t* u) const {
		return u->order_type->id == Orders::Die && u->order_state == 1;
	};

	struct contour_search {
		std::array<int, 4> inner;
		std::array<int, 4> outer;
	};

	bool contour_is_space_available(const contour_search& s, xy pos) const {

		auto cmp_u = [&](int v, paths_t::contour& c) {
			return v < c.v[0];
		};
		auto cmp_l = [&](paths_t::contour& c, int v) {
			return c.v[0] < v;
		};

		auto& c0 = st.paths.contours[0];
		for (auto i = std::upper_bound(c0.begin(), c0.end(), pos.y, cmp_u); i != c0.begin();) {
			--i;
			if (s.inner[0] + i->v[0] < pos.y) break;
			if (s.inner[1] + i->v[1] <= pos.x && s.inner[3] + i->v[2] >= pos.x) return false;
		}
		auto& c1 = st.paths.contours[1];
		for (auto i = std::lower_bound(c1.begin(), c1.end(), pos.x, cmp_l); i != c1.end(); ++i) {
			if (s.inner[1] + i->v[0] > pos.x) break;
			if (s.inner[2] + i->v[1] <= pos.y && s.inner[0] + i->v[2] >= pos.y) return false;
		}
		auto& c2 = st.paths.contours[2];
		for (auto i = std::lower_bound(c2.begin(), c2.end(), pos.y, cmp_l); i != c2.end(); ++i) {
			if (s.inner[2] + i->v[0] > pos.y) break;
			if (s.inner[1] + i->v[1] <= pos.x && s.inner[3] + i->v[2] >= pos.x) return false;
		}
		auto& c3 = st.paths.contours[3];
		for (auto i = std::upper_bound(c3.begin(), c3.end(), pos.x, cmp_u); i != c3.begin();) {
			--i;
			if (s.inner[3] + i->v[0] < pos.x) break;
			if (s.inner[2] + i->v[1] <= pos.y && s.inner[0] + i->v[2] >= pos.y) return false;
		}

		return true;

	}

	bool unit_type_can_fit_at(const unit_type_t* unit_type, xy pos) const {
		if (!is_in_map_bounds(unit_type, pos)) return false;
		if (!is_walkable(pos)) return false;
		contour_search s;
		s.inner[0] = unit_type->dimensions.from.y;
		s.outer[0] = unit_type->dimensions.from.y + 1;
		s.inner[1] = -unit_type->dimensions.to.x;
		s.outer[1] = -unit_type->dimensions.to.x - 1;
		s.inner[2] = -unit_type->dimensions.to.y;
		s.outer[2] = -unit_type->dimensions.to.y - 1;
		s.inner[3] = unit_type->dimensions.from.x;
		s.outer[3] = unit_type->dimensions.from.x + 1;
		return contour_is_space_available(s, pos);
	}

	bool unit_target_is_enemy(const unit_t* u, const unit_t* target) const {
		int n_owner = target->owner;
		if (n_owner == 11) n_owner = target->sprite->owner;
		return st.alliances[u->owner][target->owner] == 0;
	}

// 	bool some_alliance(unit_t* a, unit_t* b) {
// 		if (b->status_flags & (unit_t::status_flags_t)0x400000) {
// 			int owner = b->owner;
// 			if (owner == 11) owner = b->sprite->owner;
// 			if (!st.player_a)
// 		}
// 	}

	unit_t* get_largest_blocking_unit(unit_t* u, rect bounds) const {
		int largest_unit_area = 0;
		unit_t* largest_unit = nullptr;
		for (unit_t* nu : find_units(bounds)) {
			if (nu != u && nu->pathing_flags & 1 && !u_no_collide(nu) && unit_finder_unit_in_bounds(nu, bounds)) {
				rect n_bb = unit_type_bounding_box(nu->unit_type);
				int p = ((n_bb.to.x - n_bb.from.x) + 1) * ((n_bb.to.y - n_bb.from.y) + 1);
				if (p > largest_unit_area) {
					largest_unit_area = p;
					largest_unit = nu;
				}
			}
		}
		return largest_unit;
	}
	std::pair<bool, unit_t*> is_blocked(unit_t* u, xy pos) const {
		rect bounds = unit_bounding_box(u, pos);
		if (!is_in_map_bounds(bounds)) return std::make_pair(false, nullptr);
		unit_t* largest_unit = get_largest_blocking_unit(u, bounds);
		if (!largest_unit) return std::make_pair(!unit_type_can_fit_at(u->unit_type, pos), nullptr);
		return std::make_pair(false, largest_unit);
	}

	void set_flingy_move_target(flingy_t* f, xy move_target) {
		if (f->move_target.pos == move_target) return;
		f->move_target.pos = move_target;
		f->move_target.unit = nullptr;
		f->next_movement_waypoint = move_target;
		f->movement_flags |= 1;
	}

	void set_unit_move_target(unit_t* u, xy move_target) {
		if (u->move_target.pos == move_target) return;
		if (u->path) xcept("set_unit_move_target: fixme");
		move_target = restrict_unit_pos_to_map_bounds(move_target, u->unit_type);
		set_flingy_move_target(u, move_target);
		if (u_immovable(u)) u_unset_status_flag(u, unit_t::status_flag_immovable);
		u->recent_order_timer = 15;
		if (!u->order_queue.empty() && u->order_queue.front().order_type->unk7) {
			u_set_movement_flag(u, 0x20);
		} else {
			u_unset_movement_flag(u, 0x20);
		}
	}

	void set_current_velocity_direction(unit_t* u, direction_t current_velocity_direction) {
		if (u->current_velocity_direction == current_velocity_direction) return;
		u->current_velocity_direction = current_velocity_direction;
		u->velocity = direction_xy(current_velocity_direction, u->speed);
	}

	direction_t unit_turn_rate(const unit_t* u, direction_t desired_turn) const {
		ufp8 uturn_rate = u->flingy_turn_rate;
		if (u->flingy_movement_type != 2) uturn_rate /= 2u;
		fp8 turn_rate = fp8::truncate(uturn_rate.as_signed());
		fp8 turn = fp8::extend(desired_turn);
		if (turn > turn_rate) turn = turn_rate;
		else if (turn < -turn_rate) turn = -turn_rate;
		return direction_t::truncate(turn);
	}

	void set_desired_velocity_direction(unit_t* u, direction_t desired_velocity_direction) {
		u->desired_velocity_direction = desired_velocity_direction;
		if (u->velocity_direction != desired_velocity_direction) {
			auto turn = unit_turn_rate(u, desired_velocity_direction - u->velocity_direction);
			set_current_velocity_direction(u, u->velocity_direction + turn);
		} else {
			set_current_velocity_direction(u, desired_velocity_direction);
		}
	}

	void update_current_velocity_direction_towards_waypoint(unit_t* u) {
		if (u->position != u->next_movement_waypoint) {
			set_desired_velocity_direction(u, xy_direction(u->next_movement_waypoint - u->position));
		} else {
			if (u->position != u->next_target_waypoint) {
				set_desired_velocity_direction(u, xy_direction(u->next_target_waypoint - u->position));
			} else {
				set_desired_velocity_direction(u, u->heading);
			}
		}
	}

	void update_unit_heading(unit_t* u, direction_t velocity_direction) {
		u->velocity_direction = velocity_direction;
		if (!u_movement_flag(u, 2) || u_movement_flag(u, 1)) {
			u->heading += unit_turn_rate(u, u->desired_velocity_direction - u->heading);
			if (u->unit_type->id >= UnitTypes::Zerg_Spire && u->unit_type->id <= UnitTypes::Protoss_Robotics_Support_Bay) {
				u->flingy_turn_rate += ufp8::from_raw(1);
			} else if (u->unit_type->id >= UnitTypes::Special_Overmind && u->unit_type->id <= UnitTypes::Special_Right_Upper_Level_Door) {
				u->flingy_turn_rate += ufp8::from_raw(1);
			}
			if (velocity_direction == u->desired_velocity_direction) {
				u_unset_movement_flag(u, 1);
			}
		}
		auto heading = u->heading;
		for (image_t* image : ptr(u->sprite->images)) {
			set_image_heading(image, heading);
		}
	}

	struct execute_movement_state {
		bool refresh_vision = false;
		bool some_movement_flag0 = false;
		bool some_movement_flag1 = false;
	};

	void some_movement_func(unit_t* u, execute_movement_state& ems) {
		ems.some_movement_flag0 = false;
		ems.some_movement_flag1 = false;
		if (u_movement_flag(u, 2)) {
			u_unset_movement_flag(u, 2);
			if (!u_movement_flag(u, 8)) ems.some_movement_flag1 = true;
		} else if (u->position != u->move_target.pos) {
			if (u->flingy_movement_type != 2 || u_movement_flag(u, 8)) u_set_movement_flag(u, 2);
			if (!u_movement_flag(u, 8)) ems.some_movement_flag0 = true;
		}
	}

	bool movement_UM_Init(unit_t* u, execute_movement_state& ems) {
		u->pathing_flags &= ~(1 | 2);
		if (u->sprite->elevation_level < 12) u->pathing_flags |= 1;
		u->contour_bounds = { { 0,0 }, { 0,0 } };
		int next_state = movement_states::UM_Lump;
		if (!ut_turret(u) && u_iscript_nobrk(u)) {
			next_state = movement_states::UM_InitSeq;
		} else if (!u->sprite || unit_dead(u)) {
			// Should be unreachable, since if we get here the unit is almost certainly already destroyed.
			// If this throws, eliminate the code path that leads to it.
			xcept("unreachable?");
			next_state = movement_states::UM_Lump;
		} else if (u_in_building(u)) {
			next_state = movement_states::UM_Bunker;
		} else if (us_hidden(u)) {
			if (u_movement_flag(u, 2) || unit_movepos_state(u) == 0) {
				// SetMoveTarget_xy(u)
				// ...
				xcept("todo hidden sprite pathing stuff");
			}
			next_state = movement_states::UM_Hidden;
		} else if (u_burrowed(u)) {
			next_state = movement_states::UM_Lump;
		} else if (u_can_move(u)) {
			next_state = u->pathing_flags & 1 ? movement_states::UM_AtRest : movement_states::UM_Flyer;
		} else if (u_can_turn(u)) {
			next_state = ut_turret(u) ? movement_states::UM_Turret : movement_states::UM_BldgTurret;
		} else if (u->pathing_flags & 1 && (u_movement_flag(u, 2) || unit_movepos_state(u) == 0)) {
			next_state = movement_states::UM_LumpWannabe;
		}
		u->movement_state = next_state;
		return true;
	};

	bool movement_UM_AtRest(unit_t* u, execute_movement_state& ems) {
		if (unit_movepos_state(u) == 0) {
			if (u->pathing_collision_interval) {
				if (u->pathing_collision_interval > 2) u->pathing_collision_interval = 2;
				else --u->pathing_collision_interval;
			}
		} else u->pathing_collision_interval = 0;
		auto go_to_next_waypoint = [&]() {
			if (u_movement_flag(u, 4)) return true;
			if (unit_movepos_state(u)) {
				if (u_movement_flag(u, 2)) return true;
				if (u->position != u->next_target_waypoint) {
					auto dir = xy_direction(u->next_target_waypoint - u->position);
					if (u->heading != dir) return true;
					if (u->velocity_direction != dir) return true;
				}
			}
			return false;
		};
		bool going_to_next_waypoint = false;
		if (go_to_next_waypoint()) {
			going_to_next_waypoint = true;
			xcept("go to next waypoint!");
		}
		if (u_collision(u) && u_ground_unit(u)) {
			u->movement_state = movement_states::UM_CheckIllegal;
			return false;
		}
		if (unit_movepos_state(u) == 0 && !u_movement_flag(u, 4)) {
			u->movement_state = movement_states::UM_StartPath;
			return true;
		}
		if (!going_to_next_waypoint) {
			u->current_speed2 = 0;
			if (u->speed != fp8::zero()) {
				u->speed = fp8::zero();
				u->velocity = {};
			}
			if (u->sprite->position != u->next_target_waypoint) {
				u->next_target_waypoint = u->sprite->position;
			}
			u->movement_state = movement_states::UM_Dormant;
		}
		return false;
	}

	bool movement_UM_CheckIllegal(unit_t* u, execute_movement_state& ems) {
		u_unset_status_flag(u, unit_t::status_flag_collision);
		auto should_move_to_legal = [&]() {
			if (!u_ground_unit(u) || !is_blocked(u, u->sprite->position).first) return false;
			if (u_order_not_interruptible(u) || u_iscript_nobrk(u) || u_movement_flag(u, 8)) {
				u_set_status_flag(u, unit_t::status_flag_collision);
				return false;
			}
			xy move_to = u->sprite->position;

			if (unit_type_can_fit_at(u->unit_type, move_to)) {
				unit_t* blocking_unit = get_largest_blocking_unit(u, unit_bounding_box(u, move_to));
				if (blocking_unit) {
					xcept("should_move_to_legal: blocking unit fixme");
				}
			} else {
				xcept("should_move_to_legal: can't fit fixme");
			}

			move_to = restrict_unit_pos_to_map_bounds(move_to, u->unit_type);
			if (move_to != u->sprite->position) {
				xcept("should_move_to_legal fixme");
				return true;
			} else {
				u->status_flags |= unit_t::status_flag_collision;
				return false;
			}
		};
		if (!should_move_to_legal()) {
			u->pathing_flags &= ~(2 | 4);
			if (unit_movepos_state(u) || u_movement_flag(u, 4)) {
				u->movement_state = movement_states::UM_AtRest;
			} else {
				u->movement_state = movement_states::UM_AnotherPath;
			}
			return true;
		} else {
			u->pathing_flags |= 2;
			u->movement_state = movement_states::UM_MoveToLegal;
		}
		return false;
	}

	bool movement_UM_Dormant(unit_t* u, execute_movement_state& ems) {
		bool rest = false;
		if (u_collision(u) && u_ground_unit(u)) rest = true;
		if (unit_movepos_state(u) == 0) rest = true;
		if (u->position != u->next_target_waypoint) rest = true;
		if (rest) {
			u->movement_state = movement_states::UM_AtRest;
			return true;
		}
		return false;
	}

	bool movement_UM_Turret(unit_t* u, execute_movement_state& ems) {
		ems.refresh_vision = false;
		set_unit_move_target(u, u->sprite->position);
		auto dir_error = u->desired_velocity_direction - u->heading;
		if (dir_error != direction_t::from_raw(-128)) {
			if (dir_error >= direction_t::from_raw(-10) && dir_error <= direction_t::from_raw(10)) {
				u_unset_movement_flag(u, 1);
			}
		}
		if (u_status_flag(u, (unit_t::status_flags_t)0x2000000)) {
			some_movement_func(u, ems);
		} else {
			update_current_velocity_direction_towards_waypoint(u);
			some_movement_func(u, ems);
			update_unit_heading(u, u->velocity_direction);
		}
		return false;
	}

	bool execute_movement(unit_t* u) {

		execute_movement_state ems;
		ems.refresh_vision = update_tiles;

		while (true) {
			bool cont = false;
			switch (u->movement_state) {
			case movement_states::UM_Init:
				cont = movement_UM_Init(u, ems);
				break;
			case movement_states::UM_AtRest:
				cont = movement_UM_AtRest(u, ems);
				break;
			case movement_states::UM_CheckIllegal:
				cont = movement_UM_CheckIllegal(u, ems);
				break;
			case movement_states::UM_Dormant:
				cont = movement_UM_Dormant(u, ems);
				break;
			case movement_states::UM_Turret:
				cont = movement_UM_Turret(u, ems);
				break;
			default:
				xcept("fixme: movement state %d\n", u->movement_state);
			}
			if (!cont) break;
		}
		return ems.refresh_vision;
	};

	bool is_transforming_zerg_building(const unit_t* u) const {
		if (u_completed(u)) return false;
		unit_type_t* t = u->build_queue[u->build_queue_slot];
		if (!t) return false;
		int tt = t->id;
		return tt == UnitTypes::Zerg_Hive || tt == UnitTypes::Zerg_Lair || tt == UnitTypes::Zerg_Greater_Spire || tt == UnitTypes::Zerg_Spore_Colony || tt == UnitTypes::Zerg_Sunken_Colony;
	};


	int unit_sight_range2(const unit_t* u, bool ignore_blindness) const {
		if (u_grounded_building(u) && !u_completed(u) && !is_transforming_zerg_building(u)) return 4;
		if (!ignore_blindness && u->is_blind) return 2;
		if (u->unit_type->id == UnitTypes::Terran_Ghost && st.upgrade_levels[u->owner][UpgradeTypes::Ocular_Implants]) return 11;
		if (u->unit_type->id == UnitTypes::Zerg_Overlord && st.upgrade_levels[u->owner][UpgradeTypes::Antennae]) return 11;
		if (u->unit_type->id == UnitTypes::Protoss_Observer && st.upgrade_levels[u->owner][UpgradeTypes::Sensor_Array]) return 11;
		if (u->unit_type->id == UnitTypes::Protoss_Scout && st.upgrade_levels[u->owner][UpgradeTypes::Apial_Sensors]) return 11;
		return u->unit_type->sight_range;
	};
	int unit_sight_range(const unit_t* u) const {
		return unit_sight_range2(u, false);
	};
	int unit_sight_range_ignore_blindness(const unit_t* u) const {
		return unit_sight_range2(u, true);
	};

	int unit_target_acquisition_range(const unit_t* u) const {
		if ((u_cloaked(u) || u_requires_detector(u)) && u->order_type->id != Orders::HoldPosition) {
			if (u->unit_type->id == UnitTypes::Terran_Ghost) return 0;
			if (u->unit_type->id == UnitTypes::Hero_Sarah_Kerrigan) return 0;
			if (u->unit_type->id == UnitTypes::Hero_Alexei_Stukov) return 0;
			if (u->unit_type->id == UnitTypes::Hero_Samir_Duran) return 0;
			if (u->unit_type->id == UnitTypes::Hero_Infested_Duran) return 0;
		}
		int bonus = 0;
		if (u->unit_type->id == UnitTypes::Terran_Marine && st.upgrade_levels[u->owner][UpgradeTypes::U_238_Shells]) bonus = 1;
		if (u->unit_type->id == UnitTypes::Zerg_Hydralisk && st.upgrade_levels[u->owner][UpgradeTypes::Grooved_Spines]) bonus = 1;
		if (u->unit_type->id == UnitTypes::Protoss_Dragoon && st.upgrade_levels[u->owner][UpgradeTypes::Singularity_Charge]) bonus = 2;
		if (u->unit_type->id == UnitTypes::Hero_Fenix_Dragoon) bonus = 2;
		if (u->unit_type->id == UnitTypes::Terran_Goliath && st.upgrade_levels[u->owner][UpgradeTypes::Charon_Boosters]) bonus = 3;
		if (u->unit_type->id == UnitTypes::Terran_Goliath_Turret && st.upgrade_levels[u->owner][UpgradeTypes::Charon_Boosters]) bonus = 3;
		if (u->unit_type->id == UnitTypes::Hero_Alan_Schezar) bonus = 3;
		if (u->unit_type->id == UnitTypes::Hero_Alan_Schezar_Turret) bonus = 3;
		return u->unit_type->target_acquisition_range + bonus;
	}

	fp8 unit_max_energy(const unit_t* u) const {
		if (ut_hero(u)) return fp8::integer(250);
		auto energy_upgrade = [&]() {
			switch (u->unit_type->id) {
			case UnitTypes::Terran_Ghost: return UpgradeTypes::Moebius_Reactor;
			case UnitTypes::Terran_Wraith: return UpgradeTypes::Apollo_Reactor;
			case UnitTypes::Terran_Science_Vessel: return UpgradeTypes::Titan_Reactor;
			case UnitTypes::Terran_Battlecruiser: return UpgradeTypes::Colossus_Reactor;
			case UnitTypes::Terran_Medic: return UpgradeTypes::Caduceus_Reactor;
			case UnitTypes::Zerg_Queen: return UpgradeTypes::Gamete_Meiosis;
			case UnitTypes::Zerg_Defiler: return UpgradeTypes::Metasynaptic_Node;
			case UnitTypes::Protoss_Corsair: return UpgradeTypes::Argus_Jewel;
			case UnitTypes::Protoss_Dark_Archon: return UpgradeTypes::Argus_Talisman;
			case UnitTypes::Protoss_High_Templar: return UpgradeTypes::Khaydarin_Amulet;
			case UnitTypes::Protoss_Arbiter: return UpgradeTypes::Khaydarin_Core;
			};
			return UpgradeTypes::None;
		};
		int upg = energy_upgrade();
		if (upg != UpgradeTypes::None && st.upgrade_levels[u->owner][upg]) return fp8::integer(250);
		else return fp8::integer(200);
	}


	bool visible_to_everyone(const unit_t* u) const {
		if (ut_worker(u)) {
			if (u->worker.powerup && u->worker.powerup->unit_type->id == UnitTypes::Powerup_Flag) return true;
			else return false;
		}
		if (!u->unit_type->space_provided) return false;
		if (u->unit_type->id == UnitTypes::Zerg_Overlord && !st.upgrade_levels[u->owner][UpgradeTypes::Ventral_Sacs]) return false;
		if (u_hallucination(u)) return false;
		for (auto idx : u->loaded_units) {
			unit_t* lu = get_unit(idx);
			if (!lu || !lu->sprite) continue;
			if (unit_dead(lu)) continue;
			if (!ut_worker(lu)) continue;
			if (lu->worker.powerup && lu->worker.powerup->unit_type->id == UnitTypes::Powerup_Flag) return true;
		}
		return false;
	};

	size_t tile_index(xy pos) const {
		size_t ux = (size_t)pos.x / 32;
		size_t uy = (size_t)pos.y / 32;
		if (ux >= game_st.map_tile_width || uy >= game_st.map_tile_height) xcept("attempt to get tile index for invalid position %d %d", pos.x, pos.y);
		return uy * game_st.map_tile_width + ux;
	};

	int get_ground_height_at(xy pos) const {
		size_t index = tile_index(pos);
		tile_id creep_tile = st.gfx_creep_tiles.at(index);
		tile_id tile_id = creep_tile ? creep_tile : game_st.gfx_tiles.at(index);
		size_t megatile_index = game_st.cv5.at(tile_id.group_index()).megaTileRef[tile_id.subtile_index()];
		size_t ux = pos.x;
		size_t uy = pos.y;
		int flags = game_st.vf4.at(megatile_index).flags[uy / 8 % 4 * 4 + ux / 8 % 4];
		if (flags&MiniTileFlags::High) return 2;
		if (flags&MiniTileFlags::Middle) return 1;
		return 0;
	};

	void reveal_sight_at(xy pos, int range, int reveal_to, bool in_air) {
		int visibility_mask = ~reveal_to;
		int height_mask = 0;
		if (!in_air) {
			int height = get_ground_height_at(pos);
			if (height == 2) height_mask = tile_t::flag_very_high;
			else if (height == 1) height_mask = tile_t::flag_very_high | tile_t::flag_high;
			else height_mask = tile_t::flag_very_high | tile_t::flag_high | tile_t::flag_middle;
		}
		tile_t reveal_tile_mask;
		reveal_tile_mask.visible = visibility_mask;
		reveal_tile_mask.explored = visibility_mask;
		reveal_tile_mask.flags = 0xffff;
		tile_t required_tile_mask;
		required_tile_mask.visible = ~visibility_mask;
		required_tile_mask.explored = ~visibility_mask;
		required_tile_mask.flags = height_mask;
		auto& sight_vals = game_st.sight_values.at(range);
		size_t tile_x = (size_t)pos.x / 32;
		size_t tile_y = (size_t)pos.y / 32;
		tile_t*base_tile = &st.tiles[tile_x + tile_y*game_st.map_tile_width];
		if (!in_air) {
			auto* cur = sight_vals.maskdat.data();
			auto* end = cur + sight_vals.min_mask_size;
			for (; cur != end; ++cur) {
				cur->vision_propagation = 0xff;
				if (tile_x + cur->x >= game_st.map_tile_width) continue;
				if (tile_y + cur->y >= game_st.map_tile_height) continue;
				auto&tile = base_tile[cur->map_index_offset];
				tile.raw &= reveal_tile_mask.raw;
				cur->vision_propagation = tile.raw;
			}
			end += sight_vals.ext_masked_count;
			for (; cur != end; ++cur) {
				cur->vision_propagation = 0xff;
				if (tile_x + cur->x >= game_st.map_tile_width) continue;
				if (tile_y + cur->y >= game_st.map_tile_height) continue;
				bool okay = false;
				okay |= !(cur->prev->vision_propagation&required_tile_mask.raw);
				if (cur->prev_count == 2) okay |= !(cur->prev2->vision_propagation&required_tile_mask.raw);
				if (!okay) continue;
				auto& tile = base_tile[cur->map_index_offset];
				tile.raw &= reveal_tile_mask.raw;
				cur->vision_propagation = tile.raw;
			}
		} else {
			// This seems bugged; even for air units, if you only traverse ext_masked_count nodes,
			// then you will still miss out on the min_mask_size (9) last ones
			auto* cur = sight_vals.maskdat.data();
			auto* end = cur + sight_vals.ext_masked_count;
			for (; cur != end; ++cur) {
				if (tile_x + cur->x >= game_st.map_tile_width) continue;
				if (tile_y + cur->y >= game_st.map_tile_height) continue;
				base_tile[cur->map_index_offset].raw &= reveal_tile_mask.raw;
			}
		}
	};

	void refresh_unit_vision(unit_t*u) {
		if (u->owner >= 8 && !u->parasite_flags) return;
		if (u->unit_type->id == UnitTypes::Terran_Nuclear_Missile) return;
		int visible_to = 0;
		if (visible_to_everyone(u) || (u->unit_type->id == UnitTypes::Powerup_Flag && u->order_type->id == Orders::UnusedPowerup)) visible_to = 0xff;
		else {
			visible_to = st.shared_vision[u->owner] | u->parasite_flags;
			if (u->parasite_flags) {
				visible_to |= u->parasite_flags;
				for (size_t i = 0; i < 12; ++i) {
					if (~u->parasite_flags&(1 << i)) continue;
					visible_to |= st.shared_vision[i];
				}
			}
		}
		reveal_sight_at(u->sprite->position, unit_sight_range(u), visible_to, u_flying(u));
	};

	void turn_turret(unit_t* tu, direction_t turn) {
		if (tu->order_target.unit) u_unset_status_flag(tu, (unit_t::status_flags_t)0x2000000);
		else {
			if (tu->heading == tu->subunit->heading) u_set_status_flag(tu, (unit_t::status_flags_t)0x2000000);
		}
		if (u_status_flag(tu, (unit_t::status_flags_t)0x2000000)) set_unit_heading(tu, tu->subunit->heading);
		else {
			tu->velocity_direction = (tu->velocity_direction + turn);
			tu->heading = tu->velocity_direction;
		}
		if (tu->unit_type->id == UnitTypes::Terran_Goliath_Turret || tu->unit_type->id == UnitTypes::Hero_Alan_Schezar_Turret) {
			auto diff = tu->subunit->heading - tu->heading;
			if (diff == direction_t::from_raw(-128)) {
				tu->heading = tu->subunit->heading - direction_t::from_raw(96);
			} else if (diff > direction_t::from_raw(32)) {
				tu->heading = tu->subunit->heading - direction_t::from_raw(32);
			} else if (diff < direction_t::from_raw(-32)) {
				tu->heading = tu->subunit->heading + direction_t::from_raw(32);
			}
		}
	}

	void update_unit_movement(unit_t* u) {

		auto prev_velocity_direction = u->velocity_direction;
		bool refresh_vision = execute_movement(u);
		if (refresh_vision) refresh_unit_vision(u);

		if (u_completed(u) && u->subunit && !ut_turret(u)) {
			turn_turret(u->subunit, u->velocity_direction - prev_velocity_direction);
			u->subunit->halt = u->halt;
			u->subunit->position = { u->halt.x.integer_part(), u->halt.y.integer_part() };
			move_sprite(u->subunit->sprite, u->subunit->position);
			update_image_special_offset(u->sprite->main_image);
			iscript_unit_setter ius(this, u->subunit);
			if (!u_movement_flag(u, 2)) {
				if (u_status_flag(u->subunit, (unit_t::status_flags_t)0x1000000)) {
					u_unset_status_flag(u->subunit, (unit_t::status_flags_t)0x1000000);
					if (u_can_move(u) && !u_movement_flag(u->subunit, 8)) {
						sprite_run_anim(u->sprite, iscript_anims::WalkingToIdle);
					}
				}
			} else {
				if (!u_status_flag(u->subunit, (unit_t::status_flags_t)0x1000000)) {
					u_set_status_flag(u->subunit, (unit_t::status_flags_t)0x1000000);
					if (u_can_move(u) && !u_movement_flag(u->subunit, 8)) {
						sprite_run_anim(u->sprite, iscript_anims::Walking);
					}
				}
			}
			update_unit_movement(u->subunit);
		}
	};

	bool update_thingy_visibility(thingy_t* t, xy size) {
		if (!t->sprite || s_flag(t->sprite, sprite_t::flag_hidden)) return true;
		int tile_from_x = (t->sprite->position.x - size.x / 2) / 32;
		int tile_from_y = (t->sprite->position.y - size.y / 2) / 32;
		size_t tile_to_x = tile_from_x + (size.x + 31) / 32u;
		size_t tile_to_y = tile_from_y + (size.y + 31) / 32u;
		if (tile_from_x < 0) tile_from_x = 0;
		if (tile_from_y < 0) tile_from_y = 0;
		if (tile_to_x > game_st.map_tile_width) tile_to_x = game_st.map_tile_width;
		if (tile_to_y > game_st.map_tile_height) tile_to_y = game_st.map_tile_height;

		if (tile_from_x == tile_to_x && tile_from_y == tile_to_y) return t->sprite->visibility_flags == 0;

		uint8_t visibility_flags = 0;
		for (size_t y = tile_from_y; y < tile_to_y; ++y) {
			for (size_t x = tile_from_x; x < tile_to_y; ++x) {
				visibility_flags |= ~st.tiles[y * game_st.map_tile_width + x].visible;
			}
		}
		if (t->sprite->visibility_flags != visibility_flags) {
			set_sprite_visibility(t->sprite, visibility_flags);
			return true;
		}
		return true;
	}

	void update_unit_sprite(unit_t* u) {
		bool was_visible = (u->sprite->visibility_flags & st.local_mask) != 0;
		bool failed = update_thingy_visibility(u, u->unit_type->staredit_placement_box);
		bool is_visible = (u->sprite->visibility_flags & st.local_mask) != 0;
		if (u->subunit && !us_hidden(u->subunit)) {
			set_sprite_visibility(u->subunit->sprite, u->sprite->visibility_flags);
		}
		if (failed || (was_visible && !is_visible)) {
			// some selection stuff
			if (u_grounded_building(u) || (u->unit_type->id >= UnitTypes::Special_Floor_Missile_Trap && u->unit_type->id <= UnitTypes::Special_Right_Wall_Flame_Trap)) {
				if (!unit_dead(u)) {
					xcept("fixme create thingy");
				}
			}
		}
	};

	bool execute_hidden_unit_main_order(unit_t* u) {
		switch (u->order_type->id) {
		case Orders::Die:
			xcept("hidden Die");
			return false;
		case Orders::PlayerGuard:
			xcept("hidden PlayerGuard");
			return true;
		case Orders::TurretGuard:
			xcept("hidden TurretGuard");
			return true;
		case Orders::UnusedPowerup:
			xcept("hidden UnusedPowerup");
			return true;
		case Orders::TurretAttack:
			xcept("hidden TurretAttack");
			return true;
		case Orders::Nothing:
			return true;
		case Orders::Unused_24:
			return true;
		case Orders::InfestingCommandCenter:
			xcept("hidden InfestingCommandCenter");
			return true;
		case Orders::HarvestGas:
			xcept("hidden HarvestGas");
			return true;
		case Orders::PowerupIdle:
			xcept("hidden PowerupIdle");
			return true;
		case Orders::EnterTransport:
			xcept("hidden EnterTransport");
			return true;
		case Orders::NukeLaunch:
			xcept("hidden NukeLaunch");
			return true;
		case Orders::ResetCollision:
			xcept("hidden ResetCollision");
			return true;
		case Orders::ResetHarvestCollision:
			xcept("hidden ResetHarvestCollision");
			return true;
		case Orders::Neutral:
			return true;
		case Orders::Medic:
			return true;
		case Orders::MedicHeal:
			return true;
		}
		if (u->order_queue_timer) {
			--u->order_queue_timer;
			return true;
		}
		u->order_queue_timer = 8;
		switch (u->order_type->id) {
		case Orders::BunkerGuard:
			xcept("hidden BunkerGuard");
			break;
		case Orders::EnterTransport:
			xcept("hidden EnterTransport");
			break;
		case Orders::ComputerAI:
			xcept("hidden ComputerAI");
			break;
		case Orders::RescuePassive:
			xcept("hidden RescuePassive");
			break;
		}
		return true;
	}

	void execute_hidden_unit_secondary_order(unit_t* u) {
		switch (u->secondary_order_type->id) {
		case Orders::TrainFighter:
			xcept("hidden TrainFighter");
			break;
		case Orders::Cloak:
			xcept("hidden Cloak");
			break;
		case Orders::Decloak:
			xcept("hidden Decloak");
			break;
		}
	}

	void update_hidden_unit(unit_t* u) {
		if (u->subunit && !ut_turret(u)) {
			iscript_unit_setter ius(this, u->subunit);
			update_hidden_unit(u->subunit);
		}
		execute_movement(u);
		update_unit_values(u);

		if (!execute_hidden_unit_main_order(u)) return;
		execute_hidden_unit_secondary_order(u);

		if (u->sprite) {
			if (!iscript_execute_sprite(u->sprite)) u->sprite = nullptr;
		}

		if (!u->sprite) xcept("unit has null sprite");
	}

	int unit_calculate_visibility_flags(const unit_t* u) const {
		xcept("unit_calculate_visibility_flags: fixme");
		return 0;
	}

	void update_unit_visibilty(unit_t* u) {
		int visibility_flags = unit_calculate_visibility_flags(u);
		if (u->visibility_flags != visibility_flags) {
			xcept("update_unit_visibility: fixme");
		}
	}

	void update_units() {

		// place box/target order cursor/whatever

		--st.order_timer_counter;
		if (!st.order_timer_counter) {
			st.order_timer_counter = 150;
			int v = 0;
			for (unit_t* u : ptr(st.visible_units)) {
				u->order_queue_timer = v;
				++v;
				if (v == 8) v = 0;
			}
		}
		--st.secondary_order_timer_counter;
		if (!st.secondary_order_timer_counter) {
			st.secondary_order_timer_counter = 300;
			int v = 0;
			for (unit_t* u : ptr(st.visible_units)) {
				u->secondary_order_timer = v;
				++v;
				if (v == 30) v = 0;
			}
		}

		// some_units_loaded_and_disruption_web begin
		for (unit_t* u : ptr(st.visible_units)) {
			if (!u_flying(u) || u_status_flag(u, (unit_t::status_flags_t)0x80)) {
				u_set_status_flag(u, unit_t::status_flag_cannot_attack, false);
				if (!u_hallucination(u) && (u->unit_type->id != UnitTypes::Zerg_Overlord || st.upgrade_levels[u->owner][UpgradeTypes::Ventral_Sacs]) && u->unit_type->space_provided) {
					xcept("sub_4EB2F0 loaded unit stuff");
				} else if (u->subunit) {
					u_set_status_flag(u->subunit, unit_t::status_flag_cannot_attack, false);
				}
			}
		}
		if (st.completed_unit_counts[11][UnitTypes::Spell_Disruption_Web]) {
			xcept("disruption web stuff");
		}
		// some_units_loaded_and_disruption_web end


		for (unit_t* u : ptr(st.sight_related_units)) {
			xcept("fixme first_sight_related_unit stuff in update_units");
		}

		for (unit_t* u : ptr(st.visible_units)) {
			iscript_order_unit = u;
			iscript_unit = u;
			update_unit_movement(u);
		}

		if (update_tiles) {
			for (unit_t* u : ptr(st.scanner_sweep_units)) {
				refresh_unit_vision(u);
			}
		}

		for (unit_t* u : ptr(st.visible_units)) {
			update_unit_sprite(u);
			if (u_cloaked(u) || u_requires_detector(u)) {
				u->is_cloaked = false;
				if (u->secondary_order_timer) --u->secondary_order_timer;
				else {
					update_unit_visibilty(u);
					u->secondary_order_timer = 30;
				}
			}
		}

		for (unit_t* u : ptr(st.visible_units)) {
			iscript_order_unit = u;
			iscript_unit = u;
			update_unit(u);
		}

		for (unit_t* u : ptr(st.hidden_units)) {
			iscript_order_unit = u;
			iscript_unit = u;
			update_hidden_unit(u);
		}
		// burrowed/cloaked units
		// update_psi()
		// some lurker stuff

		for (unit_t* u : ptr(st.scanner_sweep_units)) {
			iscript_order_unit = u;
			iscript_unit = u;
			update_unit(u);
		}

		iscript_order_unit = nullptr;
		iscript_unit = nullptr;
	};

	void game_loop() {

		allow_random = true;

		if (st.update_tiles_countdown == 0) st.update_tiles_countdown = 100;
		--st.update_tiles_countdown;
		update_tiles = st.update_tiles_countdown == 0;

		update_units();

		allow_random = false;

	}

	// returns a random number in the range [0, 0x7fff]
	int lcg_rand(int source) {
		if (!allow_random) return 0;
		++st.random_counts[source];
		++st.total_random_counts;
		st.lcg_rand_state = st.lcg_rand_state * 22695477 + 1;
		return (st.lcg_rand_state >> 16) & 0x7fff;
	}
	// returns a random number in the range [from, to]
	int lcg_rand(int source, int from, int to) {
		return from + ((lcg_rand(source) * (to - from + 1)) >> 15);
	}

	void net_error_string(int str_index) {
		if (str_index) log(" error %d: (insert string here)\n", str_index);
		st.last_net_error = str_index;
	}

	void local_unit_status_error(unit_t* u, int err) {
		log("if local player, display unit status error %d\n", err);
	}

	size_t get_sprite_tile_line_index(int y) {
		int r = y / 32;
		if (r < 0) return 0;
		if ((size_t)r >= game_st.map_tile_height) return game_st.map_tile_height - 1;
		return (size_t)r;
	}
	void add_sprite_to_tile_line(sprite_t* sprite) {
		size_t index = get_sprite_tile_line_index(sprite->position.y);
		bw_insert_list(st.sprites_on_tile_line[index], *sprite);
	}
	void remove_sprite_from_tile_line(sprite_t* sprite) {
		size_t index = get_sprite_tile_line_index(sprite->position.y);
		st.sprites_on_tile_line[index].remove(*sprite);
	}

	void move_sprite(sprite_t* sprite, xy new_position) {
		if (sprite->position == new_position) return;
		size_t old_index = get_sprite_tile_line_index(sprite->position.y);
		size_t new_index = get_sprite_tile_line_index(new_position.y);
		sprite->position = new_position;
		if (old_index != new_index) {
			st.sprites_on_tile_line[old_index].remove(*sprite);
			bw_insert_list(st.sprites_on_tile_line[new_index], *sprite);
		}
	}

	void set_sprite_visibility(sprite_t* sprite, int visibility_flags) {
		if ((sprite->visibility_flags & st.local_mask) != (visibility_flags & st.local_mask)) {
			for (image_t* i : ptr(sprite->images)) i->flags |= image_t::flag_redraw;
		}
		sprite->visibility_flags = visibility_flags;
	}

	void set_image_offset(image_t* image, xy offset) {
		if (image->offset == offset) return;
		image->offset = offset;
		image->flags |= image_t::flag_redraw;
	}

	void set_image_palette_type(image_t* image, int palette_type) {
		image->palette_type = palette_type;
		if (palette_type == 17) {
			// coloring_data might be a union, since this is written
			// using two single-byte writes
			image->coloring_data = 48 | (2 << 8);
		}
		image->flags |= image_t::flag_redraw;
	}

	void set_image_palette_type(image_t* image, image_t* copy_from) {
		if (copy_from->palette_type < 2 || copy_from->palette_type > 7) return;
		set_image_palette_type(image, copy_from->palette_type);
		// seems like it's actually just two values, since this is also
		// written using two single-byte writes
		image->coloring_data = copy_from->coloring_data;
	}

	void hide_image(image_t* image) {
		if (image->flags&image_t::flag_hidden) return;
		image->flags |= image_t::flag_hidden;
	}

	void update_image_special_offset(image_t* image) {
		set_image_offset(image, get_image_lo_offset(image->sprite->main_image, 2, 0));
	}

	void update_image_frame_index(image_t* image) {
		size_t frame_index = image->frame_index_base + image->frame_index_offset;
		if (image->frame_index != frame_index) {
			image->frame_index = frame_index;
			image->flags |= image_t::flag_redraw;
		}
	}
	
	void set_image_heading(image_t* image, direction_t heading) {
		if (image->flags & image_t::flag_uses_special_offset) update_image_special_offset(image);
		if (image->flags & image_t::flag_has_directional_frames) {
			size_t frame_index_offset = (direction_index(heading) + 4) / 32;
			bool flipped = false;
			if (frame_index_offset > 16) {
				frame_index_offset = 32 - frame_index_offset;
				flipped = true;
			}
			if (image->frame_index_offset != frame_index_offset || ((image->flags & image_t::flag_horizontally_flipped) != 0) != flipped) {
				image->frame_index_offset = frame_index_offset;
				if (flipped) image->flags |= image_t::flag_horizontally_flipped;
				else image->flags &= ~image_t::flag_horizontally_flipped;
				set_image_palette_type(image, image->palette_type);
				update_image_frame_index(image);
			}
		}
	}

	void set_image_frame_index_offset(image_t* image, size_t frame_index_offset) {
		if (image->flags & image_t::flag_has_directional_frames) {
			bool flipped = false;
			if (frame_index_offset > 16) {
				frame_index_offset = 32 - frame_index_offset;
				flipped = true;
			}
			if (image->frame_index_offset != frame_index_offset || ((image->flags & image_t::flag_horizontally_flipped) != 0) != flipped) {
				image->frame_index_offset = frame_index_offset;
				if (flipped) image->flags |= image_t::flag_horizontally_flipped;
				else image->flags &= ~image_t::flag_horizontally_flipped;
				set_image_palette_type(image, image->palette_type);
				update_image_frame_index(image);
				if (image->flags & image_t::flag_uses_special_offset) update_image_special_offset(image);
			}
		}
	}

	void update_image_position(image_t* image) {

		xy map_pos = image->sprite->position + image->offset;
		auto&frame = image->grp->frames[image->frame_index];
		if (image->flags&image_t::flag_horizontally_flipped) {
			map_pos.x += image->grp->width / 2 - (frame.right + frame.left);
		} else {
			map_pos.x += frame.left - image->grp->width / 2;
		}
		if (image->flags & image_t::flag_y_frozen) map_pos.y = image->map_position.y;
		else map_pos.y += frame.top - image->grp->height / 2;
		rect grp_bounds = { { 0, 0 },{ frame.right, frame.bottom } };
		xy screen_pos = map_pos - st.viewport.from;
		if (screen_pos.x < 0) {
			grp_bounds.from.x -= screen_pos.x;
			grp_bounds.to.x += screen_pos.x;
		}
		if (screen_pos.y < 0) {
			grp_bounds.from.y -= screen_pos.y;
			grp_bounds.to.y += screen_pos.y;
		}
		if (grp_bounds.to.x > st.viewport.to.x - map_pos.x) grp_bounds.to.x = st.viewport.to.x - map_pos.x;
		if (grp_bounds.to.y > st.viewport.to.y - map_pos.y) grp_bounds.to.y = st.viewport.to.y - map_pos.y;

		image->map_position = map_pos;
		image->screen_position = screen_pos;
		image->grp_bounds = grp_bounds;

	}

	xy get_image_lo_offset(const image_t* image, int lo_index, int offset_index) const {
		int frame = image->frame_index;
		auto& lo_offsets = global_st.image_lo_offsets.at(image->image_type->id);
		if ((size_t)lo_index >= lo_offsets.size()) xcept("invalid lo index %d\n", lo_index);
		auto& frame_offsets = *lo_offsets[lo_index];
		if ((size_t)frame >= frame_offsets.size()) xcept("image %d lo_index %d does not have offsets for frame %d (frame_offsets.size() is %d)", image->image_type->id, lo_index, frame, frame_offsets.size());
		if ((size_t)offset_index >= frame_offsets[frame].size()) xcept("image %d lo_index %d frame %d does not contain an offset at index %d", image->image_type->id, lo_index, frame, offset_index);
		auto r = frame_offsets[frame][offset_index];
		if (image->flags & image_t::flag_horizontally_flipped) r.x = -r.x;
		return r;
	}

	ufp8 get_modified_unit_speed(const unit_t* u, ufp8 base_speed) const {
		ufp8 speed = base_speed;
		int mod = 0;
		if (u->stim_timer) ++mod;
		if (u_speed_upgrade(u)) ++mod;
		if (u->ensnare_timer) --mod;
		if (mod < 0) speed /= 2u;
		if (mod > 0) {
			if (u->unit_type->id == UnitTypes::Protoss_Scout || u->unit_type->id == UnitTypes::Hero_Mojo || u->unit_type->id == UnitTypes::Hero_Artanis) {
				speed = ufp8::integer(6) + (ufp8::integer(1) - ufp8::integer(1) / 3u);
			} else {
				speed += speed / 2u;
				ufp8 min_speed = ufp8::integer(3) + ufp8::integer(1) / 3u;
				if (speed < min_speed) speed = min_speed;
			}
		}
		return speed;
	}

	ufp8 get_modified_unit_acceleration(const unit_t* u, ufp8 base_acceleration) const {
		ufp8 acceleration = base_acceleration;
		int mod = 0;
		if (u->stim_timer) ++mod;
		if (u_speed_upgrade(u)) ++mod;
		if (u->ensnare_timer) --mod;
		if (mod < 0) acceleration -= acceleration / 4u;
		if (mod > 0) acceleration *= 2u;
		return acceleration;
	}

	ufp8 get_modified_unit_turn_rate(const unit_t* u, ufp8 base_turn_rate) const {
		ufp8 turn_rate = base_turn_rate;
		int mod = 0;
		if (u->stim_timer) ++mod;
		if (u_speed_upgrade(u)) ++mod;
		if (u->ensnare_timer) --mod;
		if (mod < 0) turn_rate -= turn_rate / 4u;
		if (mod > 0) turn_rate *= 2u;
		return turn_rate;
	}

	ufp8 unit_halt_distance(const unit_t* u) const {
		ufp8 speed = ufp8::from_raw(u->current_speed2);
		if (speed == ufp8::zero()) return ufp8::zero();
		if (u->flingy_movement_type != 0) return ufp8::zero();
		if (speed.raw_value == u->unit_type->flingy->top_speed && u->flingy_acceleration.raw_value == u->unit_type->flingy->acceleration) {
			return ufp8::from_raw(u->unit_type->flingy->halt_distance);
		} else {
			return ufp8::truncate(speed * speed / (u->flingy_acceleration * 2u));
		}
	}

	void iscript_set_script(image_t*image, int script_id) {
		auto i = global_st.iscript.scripts.find(script_id);
		if (i == global_st.iscript.scripts.end()) {
			xcept("script %d does not exist", script_id);
		}
		image->iscript_state.current_script = &i->second;
	}

	bool iscript_execute(image_t* image, iscript_state_t& state, bool no_side_effects = false, ufp8* distance_moved = nullptr) {

		if (state.wait) {
			--state.wait;
			return true;
		}

		auto play_frame = [&](int frame_index) {
			if (image->frame_index_base == frame_index) return;
			image->frame_index_base = frame_index;
			update_image_frame_index(image);
		};

		auto add_image = [&](int image_id, xy offset, int order) {
			log("add_image %d\n", image_id);
			const image_type_t* image_type = get_image_type(image_id);
			image_t* script_image = image;
			image_t* image = create_image(image_type, script_image->sprite, offset, order, script_image);
			if (!image) return (image_t*)nullptr;
			
			if (image->palette_type == 0 && iscript_unit && u_hallucination(iscript_unit)) {
				if (game_st.is_replay || iscript_unit->owner == game_st.local_player) {
					set_image_palette_type(image, image_t::palette_type_hallucination);
					image->coloring_data = 0;
				}
			}
			if (image->flags & image_t::flag_has_directional_frames) {
				int dir = script_image->flags & image_t::flag_horizontally_flipped ? 32 - script_image->frame_index_offset : script_image->frame_index_offset;
				set_image_frame_index_offset(image, dir);
			}
			update_image_frame_index(image);
			if (iscript_unit && (u_grounded_building(iscript_unit) || u_completed(iscript_unit))) {
				if (!image_type->draw_if_cloaked) {
					hide_image(image);
				} else if (image->palette_type==0) {
					set_image_palette_type(image, script_image);
				}
			}
			return image;
		};

		const int* program_data = global_st.iscript.program_data.data();
		const int* p = program_data + state.program_counter;
		while (true) {
			using namespace iscript_opcodes;
			size_t pc = p - program_data;
			int opc = *p++ - 0x808091;
			//log("iscript image %d (no_side_effects %d): %04x: opc %d\n", image - st.images.data(), no_side_effects, pc, opc);
			int a, b, c;
			switch (opc) {
			case opc_playfram:
				a = *p++;
				if (no_side_effects) break;
				play_frame(a);
				break;
			case opc_playframtile:
				a = *p++;
				if (no_side_effects) break;
				if ((size_t)a + game_st.tileset_index < image->grp->frames.size()) play_frame(a + game_st.tileset_index);
				break;
			case opc_sethorpos:
				a = *p++;
				if (no_side_effects) break;
				if (image->offset.x != a) {
					image->offset.x = a;
					image->flags |= image_t::flag_redraw;
				}
				break;
			case opc_setvertpos:
				a = *p++;
				if (no_side_effects) break;
				if (!iscript_unit || !(iscript_unit->status_flags&(StatusFlags::Completed | StatusFlags::GroundedBuilding))) {
					if (image->offset.y != a) {
						image->offset.y = a;
						image->flags |= image_t::flag_redraw;
					}
				}
				break;
			case opc_setpos:
				a = *p++;
				b = *p++;
				if (no_side_effects) break;
				set_image_offset(image, xy(a, b));
				break;
			case opc_wait:
				state.wait = *p++ - 1;
				state.program_counter = p - program_data;
				return true;
			case opc_waitrand:
				a = *p++;
				b = *p++;
				if (no_side_effects) break;
				state.wait = a + ((lcg_rand(3) & 0xff) % (b - a + 1)) - 1;
				state.program_counter = p - program_data;
				return true;
			case opc_goto:
				p = program_data + *p;
				break;
			case opc_imgol:
			case opc_imgul:
				a = *p++;
				b = *p++;
				c = *p++;
				if (no_side_effects) break;
				add_image(a, image->offset + xy(b, c), opc == opc_imgol ? image_order_above : image_order_below);
				break;
			case opc_imgolorig:
			case opc_switchul:
				a = *p++;
				if (no_side_effects) break;
				if (image_t* new_image = add_image(a, xy(), opc == opc_imgolorig ? image_order_above : image_order_below)) {
					if (~new_image->flags & image_t::flag_uses_special_offset) {
						new_image->flags |= image_t::flag_uses_special_offset;
						update_image_special_offset(image);
					}
				}
				break;
// 			case opc_imgoluselo:
// 				a = *p++;
// 				b = *p++;
// 				if (no_side_effects) break;
// 
// 				break;

			case opc_sprol:
				a = *p++;
				b = *p++;
				c = *p++;
				if (no_side_effects) break;
				xcept("opc_sprol");
				break;

			case opc_spruluselo:
				a = *p++;
				b = *p++;
				c = *p++;
				if (no_side_effects) break;
				xcept("opc_spruluselo");
				break;

			case opc_playsnd:
				a = *p++;
				if (no_side_effects) break;
				xcept("opc_playsnd");
				break;

			case opc_followmaingraphic:
				if (no_side_effects) break;
				if (image_t* main_image = image->sprite->main_image) {
					if (main_image->frame_index == image->frame_index && ((main_image->flags & image_t::flag_horizontally_flipped) == (image->flags & image_t::flag_horizontally_flipped))) {
						image->frame_index_base = main_image->frame_index_base;
						image->frame_index_offset = main_image->frame_index_offset;
						if (main_image->flags & image_t::flag_horizontally_flipped) image->flags |= image_t::flag_horizontally_flipped;
						else image->flags &= ~image_t::flag_horizontally_flipped;
					}
				}
				break;
			case opc_randcondjmp:
				a = *p++;
				b = *p++;
				if ((lcg_rand(7) & 0xff) <= a) {
					p = program_data + b;
				}
				break;

			case opc_turnccwise:
				a = *p++;
				if (no_side_effects) break;
				set_unit_heading(iscript_unit, iscript_unit->heading - direction_t::from_raw(8 * a));
				break;
			case opc_turncwise:
				a = *p++;
				if (no_side_effects) break;
				set_unit_heading(iscript_unit, iscript_unit->heading + direction_t::from_raw(8 * a));
				break;
			case opc_turn1cwise:
				if (no_side_effects) break;
				if (!iscript_unit->order_target.unit) set_unit_heading(iscript_unit, iscript_unit->heading + direction_t::from_raw(8));
			case opc_turnrand:
				a = *p++;
				if (no_side_effects) break;
				if (lcg_rand(6) % 4 == 1) {
					set_unit_heading(iscript_unit, iscript_unit->heading - direction_t::from_raw(8 * a));
				} else {
					set_unit_heading(iscript_unit, iscript_unit->heading + direction_t::from_raw(8 * a));
				}
				break;

			case opc_sigorder:
				a = *p++;
				if (no_side_effects) break;
				xcept("opc_sigorder");
				break;

			case opc_move:
				a = *p++;
				if (distance_moved) {
					*distance_moved = get_modified_unit_speed(iscript_unit, ufp8::integer(a));
				}
				if (no_side_effects) break;
				xcept("opc_move");
				break;

			case opc_setfldirect:
				a = *p++;
				if (no_side_effects) break;
				set_unit_heading(iscript_unit, direction_t::from_raw(a * 8));
				break;

			case opc_setflspeed:
				a = *p++;
				if (no_side_effects) break;
				xcept("opc_setflspeed");
				break;

			case opc_call:
				return true;
				break;

			case opc_orderdone:
				a = *p++;
				if (no_side_effects) break;
				xcept("opc_orderdone");
				break;

			default:
				xcept("iscript: unhandled opcode %d", opc);
			}
		}
		
	}

	bool iscript_run_anim(image_t* image, int new_anim) {
		using namespace iscript_anims;
		int old_anim = image->iscript_state.animation;
		if (new_anim == Death && old_anim == Death) return true;
		if (~image->flags & image_t::flag_has_iscript_animations && new_anim != Init && new_anim != Death) return true;
		if ((new_anim == Walking || new_anim == IsWorking) && new_anim == old_anim) return true;
		if (new_anim == GndAttkRpt && old_anim != GndAttkRpt) {
			if (old_anim != GndAttkInit) new_anim = GndAttkInit;
		}
		if (new_anim == AirAttkRpt && old_anim != AirAttkRpt) {
			if (old_anim != AirAttkInit) new_anim = AirAttkInit;
		}
		auto* script = image->iscript_state.current_script;
		if (!script) xcept("attempt to start animation without a script");
		auto& anims_pc = script->animation_pc;
		if ((size_t)new_anim >= anims_pc.size()) xcept("script %d does not have animation %d", script->id, new_anim);
		image->iscript_state.animation = new_anim;
		image->iscript_state.program_counter = anims_pc[new_anim];
		image->iscript_state.return_address = 0;
		image->iscript_state.wait = 0;
		log("image %d: iscript run anim %d pc %d\n", image - st.images.data(), new_anim, anims_pc[new_anim]);
		return iscript_execute(image, image->iscript_state);
	}

	bool iscript_execute_sprite(sprite_t* sprite) {
		for (auto i = sprite->images.begin(); i != sprite->images.end();) {
			image_t* image = &*i++;
			iscript_execute(image, image->iscript_state);
		}
		if (!sprite->images.empty()) return true;

		remove_sprite_from_tile_line(sprite);
		bw_insert_list(st.free_sprites, *sprite);
		
		return false;
	}

	void sprite_run_anim(sprite_t* sprite, int anim) {
		for (auto i = sprite->images.begin(); i != sprite->images.end();) {
			image_t* image = &*i++;
			iscript_run_anim(image, anim);
		}
	}

	void initialize_image(image_t* image, const image_type_t* image_type, sprite_t* sprite, xy offset) {
		image->image_type = image_type;
		image->grp = global_st.image_grp[image_type->id];
		int flags = 0;
		if (image_type->has_directional_frames) flags |= image_t::flag_has_directional_frames;
		if (image_type->is_clickable) flags |= 0x20;
		image->flags = flags;
		image->frame_index_base = 0;
		image->frame_index_offset = 0;
		image->frame_index = 0;
		image->sprite = sprite;
		image->offset = offset;
		image->grp_bounds = { {0,0},{0,0} };
		image->coloring_data = 0;
		image->iscript_state.current_script = nullptr;
		image->iscript_state.program_counter = 0;
		image->iscript_state.return_address = 0;
		image->iscript_state.animation = 0;
		image->iscript_state.wait = 0;
		int palette_type = image_type->palette_type;
		if (palette_type == 14) image->coloring_data = sprite->owner;
		if (palette_type == 9) {
			//images_dat.remapping[image_id];
			// some color shift stuff based on the tileset
			image->coloring_data = 0; // fixme
		}
	};

	void destroy_image(image_t* image) {
		xcept("destroy_image");
	}

	enum {
		image_order_top,
		image_order_bottom,
		image_order_above,
		image_order_below
	};
	image_t* create_image(int image_id, sprite_t* sprite, xy offset, int order, image_t* relimg = nullptr) {
		if ((size_t)image_id >= 999) xcept("attempt to create image with invalid id %d", image_id);
		return create_image(get_image_type(image_id), sprite, offset, order, relimg);
	}
	image_t* create_image(const image_type_t* image_type, sprite_t* sprite, xy offset, int order, image_t*relimg = nullptr) {
		if (!image_type)  xcept("attempt to create image of null type");
		log("create image %d\n", image_type->id);

		if (st.free_images.empty()) return nullptr;
		image_t* image = &st.free_images.front();
		st.free_images.pop_front();

		if (sprite->images.empty()) {
			sprite->main_image = image;
			sprite->images.push_front(*image);
		} else {
			if (order == image_order_top) sprite->images.push_front(*image);
			else if (order == image_order_bottom) sprite->images.push_back(*image);
			else {
				if (!relimg) relimg = sprite->main_image;
				if (order == image_order_above) sprite->images.insert(sprite->images.iterator_to(*relimg), *image);
				else sprite->images.insert(++sprite->images.iterator_to(*relimg), *image);
			}
		}
		initialize_image(image, image_type, sprite, offset);
		int palette_type = image->image_type->palette_type;
		set_image_palette_type(image, palette_type);
		if (image->image_type->has_iscript_animations) image->flags |= image_t::flag_has_iscript_animations;
		else image->flags &= image_t::flag_has_iscript_animations;
		iscript_set_script(image, image->image_type->iscript_id);
		if (!iscript_run_anim(image, iscript_anims::Init)) xcept("iscript Init ended immediately (image is no longer valid, cannot continue)");
		update_image_position(image);
		return image;
	}

	sprite_t* create_sprite(const sprite_type_t*sprite_type, xy pos, int owner) {
		if (!sprite_type)  xcept("attempt to create sprite of null type");
		log("create sprite %d\n", sprite_type->id);

		if (st.free_sprites.empty()) return nullptr;
		sprite_t* sprite = &st.free_sprites.front();
		st.free_sprites.pop_front();

		auto initialize_sprite = [&]() {
			if ((size_t)pos.x >= game_st.map_width || (size_t)pos.y >= game_st.map_height) return false;
			sprite->owner = owner;
			sprite->sprite_type = sprite_type;
			sprite->flags = 0;
			sprite->position = pos;
			sprite->visibility_flags = ~0;
			sprite->elevation_level = 4;
			sprite->selection_timer = 0;
			sprite->images.clear();
			if (!sprite_type->visible) {
				sprite->flags |= sprite_t::flag_hidden;
				set_sprite_visibility(sprite, 0);
			}
			if (!create_image(sprite_type->image, sprite, { 0,0 }, image_order_above)) return false;
			sprite->width = std::min(sprite->main_image->grp->width, 0xff);
			sprite->height = std::min(sprite->main_image->grp->width, 0xff);
			return true;
		};

		if (!initialize_sprite()) {
			bw_insert_list(st.free_sprites, *sprite);
			return nullptr;
		}
		add_sprite_to_tile_line(sprite);

		return sprite;
	}

	bool initialize_flingy(flingy_t* f, const flingy_type_t* flingy_type, xy pos, int owner, direction_t direction) {
		f->flingy_type = flingy_type;
		f->movement_flags = 0;
		f->current_speed2 = 0;
		f->flingy_top_speed = ufp8::from_raw(flingy_type->top_speed);
		f->flingy_acceleration = ufp8::from_raw(flingy_type->acceleration);
		f->flingy_turn_rate = ufp8::from_raw(flingy_type->turn_rate);
		f->flingy_movement_type = flingy_type->movement_type;

		f->position = pos;
		f->halt = { fp8::integer(pos.x), fp8::integer(pos.y) };

		set_flingy_move_target(f, pos);
		if (f->next_target_waypoint != pos) {
			f->next_target_waypoint = pos;
		}
		f->heading = direction;
		f->velocity_direction = direction;

		f->sprite = create_sprite(flingy_type->sprite, pos, owner);
		if (!f->sprite) return false;
		auto dir = f->heading;
		for (image_t* i : ptr(f->sprite->images)) {
			set_image_heading(i, dir);
		}

		return true;
	}

	void update_unit_speed_upgrades(unit_t*u) {
		auto speed_upgrade = [&]() {
			switch (u->unit_type->id) {
			case UnitTypes::Terran_Vulture:
			case UnitTypes::Hero_Jim_Raynor_Vulture:
				return UpgradeTypes::Ion_Thrusters;
			case UnitTypes::Zerg_Overlord:
				return UpgradeTypes::Pneumatized_Carapace;
			case UnitTypes::Zerg_Zergling:
				return UpgradeTypes::Metabolic_Boost;
			case UnitTypes::Zerg_Hydralisk:
				return UpgradeTypes::Muscular_Augments;
			case UnitTypes::Protoss_Zealot:
				return UpgradeTypes::Leg_Enhancements;
			case UnitTypes::Protoss_Scout:
				return UpgradeTypes::Gravitic_Thrusters;
			case UnitTypes::Protoss_Shuttle:
				return UpgradeTypes::Gravitic_Drive;
			case UnitTypes::Protoss_Observer:
				return UpgradeTypes::Gravitic_Boosters;
			case UnitTypes::Zerg_Ultralisk:
				return UpgradeTypes::Anabolic_Synthesis;
			};
			return UpgradeTypes::None;
		};
		bool cooldown = false;
		if (u->unit_type->id == UnitTypes::Hero_Devouring_One) cooldown = true;
		if (u->unit_type->id == UnitTypes::Zerg_Zergling && st.upgrade_levels[u->owner][UpgradeTypes::Adrenal_Glands]) cooldown = true;
		bool speed = false;
		int speed_upg = speed_upgrade();
		if (speed_upg != UpgradeTypes::None && st.upgrade_levels[u->owner][speed_upg]) speed = true;
		if (u->unit_type->id == UnitTypes::Hero_Hunter_Killer) speed = true;
		if (u->unit_type->id == UnitTypes::Hero_Yggdrasill) speed = true;
		if (u->unit_type->id == UnitTypes::Hero_Fenix_Zealot) speed = true;
		if (u->unit_type->id == UnitTypes::Hero_Mojo) speed = true;
		if (u->unit_type->id == UnitTypes::Hero_Artanis) speed = true;
		if (u->unit_type->id == UnitTypes::Zerg_Lurker) speed = true;
		if (cooldown != u_cooldown_upgrade(u) || speed != u_speed_upgrade(u)) {
			if (cooldown) u->status_flags |= unit_t::status_flag_cooldown_upgrade;
			if (speed) u->status_flags |= unit_t::status_flag_speed_upgrade;
			update_unit_speed(u);
		}
	}

	void update_unit_speed(unit_t*u) {
		
		int movement_type = u->unit_type->flingy->movement_type;
		if (movement_type != 0 && movement_type != 1) {
			if (u->flingy_movement_type == 2) {
				image_t* image = u->sprite->main_image;
				if (!image) xcept("null image");
				auto* script = image->iscript_state.current_script;
				auto& anims_pc = script->animation_pc;
				int anim = iscript_anims::Walking;
				// BW just returns if the animation doesn't exist,
				// so this could be changed to a return statement if it throws
				if ((size_t)anim >= anims_pc.size()) xcept("script %d does not have animation %d", script->id, anim);
				iscript_unit_setter ius(this, u);
				iscript_state_t st;
				st.current_script = script;
				st.animation = anim;
				st.program_counter = anims_pc[anim];
				st.return_address = 0;
				st.wait = 0;
				ufp8 total_distance_moved {};
				for (int i = 0; i < 32; ++i) {
					ufp8 distance_moved {};
					iscript_execute(image, st, true, &distance_moved);
					// This get_modified_unit_acceleration is very out of place, and
					// it makes the stored flingy_top_speed value wrong. But BroodWar does it.
					// It's probably a bug, but the value might not be used for anything
					// significant.
					total_distance_moved += get_modified_unit_acceleration(u, distance_moved);
				}
				auto avg_distance_moved = total_distance_moved / 32u;
				u->flingy_top_speed = avg_distance_moved;
			}
		} else {
			u->flingy_top_speed = get_modified_unit_speed(u, ufp8::from_raw(u->unit_type->flingy->top_speed));
			u->flingy_acceleration = get_modified_unit_acceleration(u, ufp8::from_raw(u->unit_type->flingy->acceleration));
			u->flingy_turn_rate = get_modified_unit_turn_rate(u, ufp8::from_raw(u->unit_type->flingy->turn_rate));
		}

	}

	void increment_unit_counts(unit_t* u, int count) {
		if (u_hallucination(u)) return;
		if (ut_turret(u)) return;

		st.unit_counts[u->owner][u->unit_type->id] += count;
		int supply_required = u->unit_type->supply_required;
		if (u->unit_type->staredit_group_flags & GroupFlags::Zerg) {
			const unit_type_t*ut = u->unit_type;
			if (ut->id==UnitTypes::Zerg_Egg || ut->id==UnitTypes::Zerg_Cocoon || ut->id==UnitTypes::Zerg_Lurker_Egg) {
				ut = u->build_queue[u->build_queue_slot];
				supply_required = ut->supply_required;
				if (ut_two_units_in_one_egg(u)) supply_required *= 2;
			} else {
				if (ut_flyer(u) && !u_completed(u)) supply_required *= 2;
			}
			st.supply_used[0][u->owner] += supply_required * count;
		} else if (u->unit_type->staredit_group_flags & GroupFlags::Terran) {
			st.supply_used[1][u->owner] += supply_required * count;
		} else if (u->unit_type->staredit_group_flags & GroupFlags::Protoss) {
			st.supply_used[2][u->owner] += supply_required * count;
		}
		if (u->unit_type->staredit_group_flags & GroupFlags::Factory) st.factory_counts[u->owner] += count;
		if (u->unit_type->staredit_group_flags & GroupFlags::Men) {
			st.non_building_counts[u->owner] += count;
		} else if (u->unit_type->staredit_group_flags & GroupFlags::Building) st.building_counts[u->owner] += count;
		else if (u->unit_type->id == UnitTypes::Zerg_Egg || u->unit_type->id == UnitTypes::Zerg_Cocoon || u->unit_type->id == UnitTypes::Zerg_Lurker_Egg) {
			st.non_building_counts[u->owner] += count;
		}
		if (st.unit_counts[u->owner][u->unit_type->id] < 0) st.unit_counts[u->owner][u->unit_type->id] = 0;
	}

	// 
	// Unit finder --
	// In Broodwar the unit finder works by keeping all units in two sorted vectors, one for x and one for y.
	// Each unit is inserted into each vector twice with the top left and bottom right coordinates respectively.
	// So a unit is added to x with the left and right values, and into y with the top and bottom.
	// When inserting into x or y it inserts the unit at the lower bound index (keeping it sorted).
	// To find units (within a rectangle), it does a lower bound search for the left, right, top and bottom values,
	// then it iterates [left index, right index) in x, marks all units, then iterates [top index, bottom index) in
	// y and marks again. Then it reiterates the ones in x and only returns the ones that were double marked.
	// In order to work properly when the search area is smaller than the unit size, it forces the search area
	// to be at least as large as the largest unit type in the game, by extending right and bottom, and then
	// doing an additional bounds check when iterating to make sure it only returns units in the original search area.
	// Otherwise, no bounds check is performed (as a performance optimization) since the indices already match the search area.
	// 
	// This means the order units are returned are essentially sorted as follows, where a and b are imaginary structures
	// where a.from is the upper left and a.to is the bottom right of the unit bounding box, area is the search area,
	// and a.insert_order is a unique incremental value that is set each time a unit is inserted:
	//   int ax = a.from.x >= area.from.x ? a.from.x : a.to.x;
	//   int bx = b.from.x >= area.from.x ? b.from.x : b.to.x;
	//   if (ax == bx) return a.insert_order > b.insert_order;
	//   else return ax < bx;
	// 
	// In other words, they are sorted by leftmost x if it is within the area, otherwise rightmost x, and after that
	// by reverse insertion order.
	// 
	// Now, as far as I can tell, the search area is supposed to be inclusive, so if you search from [32,32] to [64,64]
	// and some unit's bounding box is from [0,0] to [32,32] then that unit should be returned (unit bounding boxes
	// seem to be inclusive in all directions).
	// This seems to make sense since searches are initiated for instance by the unit bounding box to find collisions.
	// The right index and bottom index is found by lower bound lookup, which would give an exclusive search,
	// but the additional bounds check for small searches is inclusive.
	// Since the bounds check is only performed for searches smaller than the largest unit size, the result is inclusive
	// for those searches and exclusive for the larger ones (only for the right and bottom coordinates. Left and top is
	// always inclusive). The x and y axis are treated individually, so it can be inclusive in one and exclusive in the
	// other. This would be easily fixed by doing a upper bound search for the right and bottom indices (then it would
	// always be inclusive).
	// The largest unit size is 256x160.
	//
	// Note: There appear to be two search methods, the above paragraph only applies to one of them. The second method
	// only does a lookup on the left and top coordinates, then iterates and does exclusive bounds checking. Thus, the
	// only difference is the special case for small search areas (it is not present in the second method).
	//  
	// The order units are found is important sometimes, but not other times. For instance in code where we select
	// one unit that matches some criterion, it is critically important that we select the right one where multiple
	// might match. The easiest way to do this is to iterate through units in the same order as Broodwar.
	// Another way to do it would be to store the insertion order in each unit and then when multiple units match
	// some criterion we select the correct one based on the sorting function above. Then we would not be able to
	// break the iteration early, and also adding the logic to each search might be inconvenient or even slow
	// in some cases.
	// 
	// To return units in the same order with good performance, we pretty much need to use the same method,
	// however we do not need to keep a sorted list on the y axis. I believe dropping the y axis and just doing
	// bounds check when iterating is equally fast or faster, and insert/erase is twice as fast.
	// It would be possible to use a binary search tree like a red-black tree instead of a sorted vector, but
	// iteration would be slower and for small unit counts insert/erase would be slower too. Also they typically
	// iterate equal elements in insertion order, but we need reverse insertion order, though that's easy to fix.
	//
	//
	// Okay, so the implementation here splits the x axis up into groups of state::unit_finder_group_size pixels,
	// keeping one sorted vector for each. When inserting we just insert into the lower bound index of the appropriate
	// group. To make iteration fast, each entry has a pointer to the next entry (in sorted order). The next entry
	// might be in a different group and empty groups are skipped.
	// 
	// We expand the search in the same way as Broodwar, and take care to perform the bounds checking in the same
	// way. Since we don't keep a sorted vector of y values, we specifically do an inclusive or exclusive bounds check
	// based on whether the search height was smaller than the largest unit height.
	// (this is currently incorrect for the second method, fixme?)
	// 
	// It might be worth considering maintaining two different unit finders, one ordered and one unordered, or even
	// just an unordered one and then doing the additional work mentioned above when the order matters.
	// Will have to see later in the development after the number of searches has gone up.
	// 

	void unit_finder_insert(unit_t* u) {
		if (ut_turret(u)) return;

		rect bb = unit_sprite_bounding_box(u);
		unit_finder_insert(u, bb);
	}

	state::unit_finder_entry* unit_finder_prev_entry(size_t index) const {
		while (index--) {
			if (!st.unit_finder_groups[index].empty()) return &st.unit_finder_groups[index].back();
		}
		return nullptr;
	};
	state::unit_finder_entry* unit_finder_next_entry(size_t index) const {
		while (++index != st.unit_finder_groups.size()) {
			if (!st.unit_finder_groups[index].empty()) return &st.unit_finder_groups[index].front();
		}
		return nullptr;
	};

	auto unit_finder_prev_entry_iterator(size_t index) const {
		auto* e = unit_finder_prev_entry(index);
		if (e) return st.unit_finder_list.iterator_to(*e);
		else return st.unit_finder_list.begin();
	}
	auto unit_finder_next_entry_iterator(size_t index) const {
		auto* e = unit_finder_next_entry(index);
		if (e) return st.unit_finder_list.iterator_to(*e);
		else return st.unit_finder_list.end();
	}

	void unit_finder_insert(unit_t* u, rect bb) {

		u->unit_finder_bounding_box = bb;

		size_t index_from = (size_t)bb.from.x / state::unit_finder_group_size;
		size_t index_to = ((size_t)bb.to.x + state::unit_finder_group_size - 1) / state::unit_finder_group_size;
		if (index_from >= st.unit_finder_groups.size() || index_to >= st.unit_finder_groups.size()) {
			xcept("unit is outside map? bb %d %d %d %d - remove me if this throws, just curious if it can happen", bb.from.x, bb.from.y, bb.to.x, bb.to.y);
			if (bb.from.x <= 0) index_from = 0;
			else if ((size_t)bb.from.x >= game_st.map_width) index_from = st.unit_finder_groups.size() - 1;
			if (bb.to.x <= 0) index_to = 0;
			else if ((size_t)bb.to.x >= game_st.map_width) index_to = st.unit_finder_groups.size() - 1;
		}

		log("insert, index_from is %d, index_to is %d\n", index_from, index_to);

		auto insert = [this](size_t index, unit_t* u, int value) {
			auto& vec = st.unit_finder_groups[index];
			auto i_from = std::lower_bound(vec.begin(), vec.end(), value, [](auto& a, int value) {
				return a.value < value;
			});

			size_t new_size = vec.size() + 1;
			if (new_size <= vec.capacity()) {
				if (vec.empty() || i_from == vec.end()) {
					vec.push_back({ u, value });
					auto next_i = unit_finder_next_entry_iterator(index);
					st.unit_finder_list.insert(next_i, vec.back());
				} else {
					auto next_i = ++st.unit_finder_list.iterator_to(vec.back());
					vec.push_back(vec.back());
					st.unit_finder_list.insert(next_i, vec.back());
					auto i_from_next = i_from;
					++i_from_next;
					if (i_from_next != vec.end()) {
						for (auto i = --vec.end(); i != i_from_next;) {
							unit_t* u = i->u;
							int value = i->value;
							--i;
							i->u = u;
							i->value = value;
						}
					}
					i_from->u = u;
					i_from->value = value;
				}
			} else {
				if (i_from == vec.end()) {
					vec.push_back({ u, value });
					auto next_i = unit_finder_next_entry_iterator(index);
					st.unit_finder_list.insert(next_i, vec.back());
				} else {
					auto next_i = ++st.unit_finder_list.iterator_to(vec.back());
					for (auto& v : vec) st.unit_finder_list.remove(v);
					vec.insert(i_from, { u, value });
					for (auto& v : vec) st.unit_finder_list.insert(next_i, v);
				}
			}
		};

		insert(index_from, u, bb.from.x);
		insert(index_to, u, bb.to.x);
		
	}
	struct unit_finder_search {
		using value_type = unit_t*;
		
		struct iterator {
			using value_type = unit_t*;
			using iterator_category = std::forward_iterator_tag;
		private:
			state::unit_finder_list_iterator i;
			state::unit_finder_list_iterator end_i;
			unit_finder_search& search;
			friend state_functions;
			iterator(state::unit_finder_list_iterator i, unit_finder_search& search) : i(i), search(search) {}
			bool in_bounds() {
				unit_t* u = i->u;
				if (u->unit_finder_bounding_box.from.x > search.bb.to.x) return false;
				if (u->unit_finder_bounding_box.to.y < search.bb.from.y) return false;
				if (u->unit_finder_bounding_box.from.y > search.bb.to.y) return false;
				return true;
			};

		public:

			unit_t* operator*() const {
				return i->u;
			}

			unit_t* operator->() const {
				return i->u;
			}

			iterator& operator++() {
				do {
					++i;
				} while (i != search.i_end && (!in_bounds() || i->u->unit_finder_visited));
				i->u->unit_finder_visited = true;
				return *this;
			}

			iterator operator++(int) {
				auto r = *this;
				++*this;
				return r;
			}

			bool operator==(const iterator& n) const {
				return i == n.i;
			}
			bool operator!=(const iterator& n) const {
				return i != n.i;
			}
		};

	private:
		friend state_functions;
		const state_functions& funcs;
		state::unit_finder_list_iterator i_begin;
		state::unit_finder_list_iterator i_end;
		rect bb;
		unit_finder_search(const state_functions& funcs) : funcs(funcs) {
			if (funcs.unit_finder_search_active) xcept("recursive unit_finder_search is not supported");
			funcs.unit_finder_search_active = true;
		}
	public:
		~unit_finder_search() {
			funcs.unit_finder_search_active = false;
		}

		iterator begin() {
			return iterator(i_begin, *this);
		}
		iterator end() {
			return iterator(i_end, *this);
		}
	};

	unit_finder_search find_units(rect area) const {
		unit_finder_search r(*this);

		int index_from_x = area.from.x;
		int index_to_x = area.to.x;
		if (area.to.x - area.from.x + 1 < game_st.max_unit_width) {
			index_to_x = area.from.x + game_st.max_unit_width - 1;
		} else ++area.to.x;
		if (area.to.y - area.from.y + 1 < game_st.max_unit_height) {
		} else ++area.to.y;

		size_t index_from = (size_t)index_from_x / state::unit_finder_group_size;
		size_t index_to = (size_t)index_to_x / state::unit_finder_group_size;
		if (index_from >= st.unit_finder_groups.size()) {
			if (index_from_x <= 0) index_from = 0;
			else index_from = st.unit_finder_groups.size() - 1;
		}
		if (index_to >= st.unit_finder_groups.size()) {
			if (index_to_x <= 0) index_to = 0;
			else index_to = st.unit_finder_groups.size() - 1;
		}

		r.bb = area;

		auto& vec_from = st.unit_finder_groups[index_from];
		if (!vec_from.empty()) {
			auto i = std::lower_bound(vec_from.begin(), vec_from.end(), area.from.x, [](auto& a, int value) {
				return a.value < value;
			});
			if (i == vec_from.end()) r.i_begin = unit_finder_next_entry_iterator(index_from);
			else r.i_begin = st.unit_finder_list.iterator_to(*i);
		} else r.i_begin = unit_finder_next_entry_iterator(index_from);
		r.i_end = unit_finder_next_entry_iterator(index_to);

		for (auto i = r.i_begin; i != r.i_end; ++i) {
			i->u->unit_finder_visited = false;
		}
		if (r.i_begin != r.i_end) r.i_begin->u->unit_finder_visited = true;

		return r;
	}

	template<typename F>
	unit_t* find_unit(rect area, const F& predicate) const {
		for (unit_t* u : find_units(area)) {
			if (predicate(u)) return u;
		}
		return nullptr;
	}



	bool initialize_unit_type(unit_t*u, const unit_type_t*unit_type, xy pos, int owner) {

		iscript_unit_setter ius(this, u);
		if (!initialize_flingy(u, unit_type->flingy, pos, owner, direction_t::zero())) return false;

		u->owner = owner;
		u->order_type = get_order_type(Orders::Fatal);
		u->order_state = 0;
		u->order_signal = 0;
		u->main_order_timer = 0;
		u->ground_weapon_cooldown = 0;
		u->air_weapon_cooldown = 0;
		u->spell_cooldown = 0;
		u->order_target.unit = nullptr;
		u->order_target.pos = { 0,0 };
		u->unit_type = unit_type;
		u->resource_type = 0;
		u->secondary_order_timer = 0;
		
		if (!iscript_execute_sprite(u->sprite)) {
			xcept("initialize_unit_type: iscript removed the sprite (if this throws, then Broodwar would crash)");
			u->sprite = nullptr;
		}
		u->last_attacking_player = 8;
		u->shield_points = fp8::integer(u->unit_type->shield_points);
		if (u->unit_type->id == UnitTypes::Protoss_Shield_Battery) u->energy = fp8::integer(100);
		else u->energy = unit_max_energy(u) / 4;
		// u->ai_action_flag = 0;
		u->sprite->elevation_level = unit_type->elevation_level;
		u_set_status_flag(u, unit_t::status_flag_grounded_building, ut_building(u));
		u_set_status_flag(u, unit_t::status_flag_flying, ut_flyer(u));
		u_set_status_flag(u, unit_t::status_flag_can_turn, ut_can_turn(u));
		u_set_status_flag(u, unit_t::status_flag_can_move, ut_can_move(u));
		u_set_status_flag(u, unit_t::status_flag_ground_unit, !ut_flyer(u));
		if (u->unit_type->elevation_level < 12) u->pathing_flags |= 1;
		else u->pathing_flags &= ~1;
		if (ut_building(u)) {
			u->building.addon = nullptr;
			u->building.addon_build_type = nullptr;
			u->building.upgrade_research_time = 0;
			u->building.tech_type = nullptr;
			u->building.upgrade_type = nullptr;
			u->building.larva_timer = 0;
			u->building.landing_timer = 0;
			u->building.creep_timer = 0;
			u->building.upgrade_level = 0;
		}
		u->path = nullptr;
		u->movement_state = 0;
		u->recent_order_timer = 0;
		u_set_status_flag(u, unit_t::status_flag_invincible, ut_invincible(u));

		// u->building_overlay_state = 0; xcept("fixme building overlay state needs damage graphic?");

		if (u->unit_type->build_time == 0) {
			u->remaining_build_time = 1;
			u->hp_construction_rate = fp8::integer(1) / 256;
		} else {
			u->remaining_build_time = u->unit_type->build_time;
			u->hp_construction_rate = (u->unit_type->hitpoints - u->unit_type->hitpoints / 10 + fp8::integer(u->unit_type->build_time) / 256 - fp8::integer(1) / 256) / u->unit_type->build_time;
			if (u->hp_construction_rate == fp8::zero()) u->hp_construction_rate = fp8::integer(1) / 256;
		}
		if (u->unit_type->has_shield && u_grounded_building(u)) {
			fp8 max_shields = fp8::integer(u->unit_type->shield_points);
			u->shield_points = max_shields / 10;
			if (u->unit_type->build_time == 0) {
				u->shield_construction_rate = fp8::integer(1);
			} else {
				u->shield_construction_rate = (max_shields - u->shield_points) / u->unit_type->build_time;
				if (u->shield_construction_rate == fp8::zero()) u->shield_construction_rate = fp8::integer(1);
			}
		}
		update_unit_speed_upgrades(u);
		update_unit_speed(u);

		return true;
	}

	void destroy_unit(unit_t* u) {
		xcept("destroy_unit\n");
	}

	unit_t* create_unit(const unit_type_t* unit_type, xy pos, int owner) {
		if (!unit_type) xcept("attempt to create unit of null type");

		lcg_rand(14);
		auto get_new = [&](const unit_type_t* unit_type) {
			if (st.free_units.empty()) {
				net_error_string(61); // Cannot create more units
				return (unit_t*)nullptr;
			}
			if (!is_in_map_bounds(unit_type, pos)) {
				net_error_string(0);
				return (unit_t*)nullptr;
			}
			unit_t* u = &st.free_units.front();
			auto initialize_unit = [&]() {

				u->order_queue.clear();

				u->auto_target_unit = nullptr;
				u->connected_unit = nullptr;

				u->order_queue_count = 0;
				u->order_queue_timer = 0;
				u->unknown_0x086 = 0;
				u->attack_notify_timer = 0;
				u->displayed_unit_id = 0;
				u->last_event_timer = 0;
				u->last_event_color = 0;
				//u->unused_0x08C = 0;
				u->rank_increase = 0;
				u->kill_count = 0;

				u->remove_timer = 0;
				u->defense_matrix_damage = 0;
				u->defense_matrix_timer = 0;
				u->stim_timer = 0;
				u->ensnare_timer = 0;
				u->lockdown_timer = 0;
				u->irradiate_timer = 0;
				u->stasis_timer = 0;
				u->plague_timer = 0;
				u->storm_timer = 0;
				u->irradiated_by = nullptr;
				u->irradiate_owner = 0;
				u->parasite_flags = 0;
				u->cycle_counter = 0;
				u->is_blind = 0;
				u->maelstrom_timer = 0;
				u->unused_0x125 = 0;
				u->acid_spore_count = 0;
				for (auto& v : u->acid_spore_time) v = 0;
				u->status_flags = 0;
				u->user_action_flags = 0;
				u->pathing_flags = 0;
				u->previous_hp = 1;
				u->ai = nullptr;

				if (!initialize_unit_type(u, unit_type, pos, owner)) return false;

				u->build_queue.fill(nullptr);
				u->unit_id_generation = (u->unit_id_generation + 1) % (1 << 5);
				auto produces_units = [&]() {
					if (u->unit_type->id == UnitTypes::Terran_Command_Center) return true;
					if (u->unit_type->id == UnitTypes::Terran_Barracks) return true;
					if (u->unit_type->id == UnitTypes::Terran_Factory) return true;
					if (u->unit_type->id == UnitTypes::Terran_Starport) return true;
					if (u->unit_type->id == UnitTypes::Zerg_Infested_Command_Center) return true;
					if (u->unit_type->id == UnitTypes::Zerg_Hatchery) return true;
					if (u->unit_type->id == UnitTypes::Zerg_Lair) return true;
					if (u->unit_type->id == UnitTypes::Zerg_Hive) return true;
					if (u->unit_type->id == UnitTypes::Protoss_Nexus) return true;
					if (u->unit_type->id == UnitTypes::Protoss_Gateway) return true;
					return false;
				};
				if (!is_frozen(u) || u_completed(u)) {
					if (produces_units()) u->current_button_set = UnitTypes::Factories;
					else u->current_button_set = UnitTypes::Buildings;
				}
				u->wireframe_randomizer = lcg_rand(15);
				if (ut_turret(u)) u->hp = fp8::integer(1) / 256;
				else u->hp = u->unit_type->hitpoints / 10;
				if (u_grounded_building(u)) u->order_type = get_order_type(Orders::Nothing);
				else u->order_type = u->unit_type->human_ai_idle;
				// secondary_order_id is uninitialized
				if (!u->secondary_order_type || u->secondary_order_type->id != Orders::Nothing) {
					u->secondary_order_type = get_order_type(Orders::Nothing);
					u->secondary_order_unk_a = 0;
					u->secondary_order_unk_b = 0;
					u->current_build_unit = nullptr;
					u->secondary_order_state = 0;
				}
				u->unit_finder_bounding_box = { {-1, -1}, {-1, -1} };
				st.player_units[owner].push_front(*u);
				increment_unit_counts(u, 1);

				if (u_grounded_building(u)) {
					unit_finder_insert(u);
				} else {
					if (unit_type->id==UnitTypes::Terran_Vulture || unit_type->id==UnitTypes::Hero_Jim_Raynor_Vulture) {
						u->vulture.spider_mine_count = 0;
					}
					u->sprite->flags |= sprite_t::flag_hidden;
					set_sprite_visibility(u->sprite, 0);
				}
				u->visibility_flags = ~0;
				if (ut_turret(u)) {
					u->sprite->flags |= 0x10;
				} else {
					if (!us_hidden(u)) {
						refresh_unit_vision(u);
					}
				}

				return true;
			};
			if (!initialize_unit()) {
				net_error_string(62); // Unable to create unit
				return (unit_t*)nullptr;
			}
			st.free_units.pop_front();
			return u;
		};
		unit_t* u = get_new(unit_type);
		if (u_grounded_building(u)) bw_insert_list(st.visible_units, *u);
		else bw_insert_list(st.hidden_units, *u);

		if (unit_type->id < UnitTypes::Terran_Command_Center && unit_type->turret_unit_type) {
			unit_t* su = get_new(unit_type->turret_unit_type);
			if (!su) {
				destroy_unit(u);
				return nullptr;
			}
			u->subunit = su;
			su->subunit = u;
			set_image_offset(u->sprite->main_image, get_image_lo_offset(u->sprite->main_image, 2, 0));
			if (ut_turret(u)) xcept("unit %d has a turret but is also flagged as a turret", u->unit_type->id);
			if (!ut_turret(su)) xcept("unit %d was created as a turret but is not flagged as one", su->unit_type->id);
		} else {
			u->subunit = nullptr;
		}

		return u;
	}

	unit_t* create_unit(int unit_type_id, xy pos, int owner) {
		if ((size_t)unit_type_id >= 228) xcept("attempt to create unit with invalid id %d", unit_type_id);
		return create_unit(get_unit_type(unit_type_id), pos, owner);
	}

	void replace_sprite_images(sprite_t* sprite, const image_type_t* new_image_type, direction_t heading) {
		// selection stuff...

		for (auto i = sprite->images.begin(); i != sprite->images.end();) {
			image_t* image = &*i++;
			destroy_image(image);
		}

		create_image(new_image_type, sprite, {}, image_order_above);

		// selection stuff...

		for (image_t* img : ptr(sprite->images)) {
			set_image_heading(img, heading);
		}
		
	}

	void apply_unit_effects(unit_t*u) {
		if (u->defense_matrix_timer) {
			xcept("apply_defensive_matrix");
		}
		if (u->lockdown_timer) {
			u->lockdown_timer = 0;
			xcept("lockdown_hit");
		}
		if (u->maelstrom_timer) {
			u->maelstrom_timer = 0;
			xcept("set_maelstrom_timer");
		}
		if (u->irradiate_timer) {
			xcept("apply_irradiate");
		}
		if (u->ensnare_timer) {
			u->ensnare_timer = 0;

		}
	}

	void set_construction_graphic(unit_t* u, bool animated) {

		bool requires_detector_or_cloaked = u_requires_detector(u) || u_cloaked(u);
		int coloring_data = 0;
		if (requires_detector_or_cloaked) {
			coloring_data = u->sprite->main_image->coloring_data;
		}
		iscript_unit_setter ius(this, u);
		const image_type_t* construction_image = u->unit_type->construction_animation;
		if (!animated || !construction_image) construction_image = u->sprite->sprite_type->image;
		replace_sprite_images(u->sprite, construction_image, u->heading);

		if (requires_detector_or_cloaked) {
			// some stuff...
		}

		apply_unit_effects(u);

	}

	void set_unit_heading(unit_t* u, direction_t heading) {
		u->velocity_direction = heading;
		u->heading = heading;
		u->current_velocity_direction = heading;
		u->velocity = direction_xy(heading, u->speed);
		if (u->next_target_waypoint != u->sprite->position) {
			u->next_target_waypoint = u->sprite->position;
		}
		for (image_t* img : ptr(u->sprite->images)) {
			set_image_heading(img, heading);
		}

	}

	void finish_building_unit(unit_t*u) {
		if (u->remaining_build_time) {
			u->hp = u->unit_type->hitpoints;
			u->shield_points = fp8::integer(u->unit_type->shield_points);
			u->remaining_build_time = 0;
		}
		set_current_button_set(u, u->unit_type->id);
		if (u_grounded_building(u)) {
			u->parasite_flags = 0;
			u->is_blind = false;
			set_construction_graphic(u, false);
		} else {
			if (u_can_turn(u)) {
				int dir = u->unit_type->unit_direction;
				if (dir == 32) dir = lcg_rand(36) % 32;
				set_unit_heading(u, direction_t::from_raw(dir * 8));
			}
			if (u->unit_type->id >= UnitTypes::Special_Floor_Missile_Trap && u->unit_type->id <= UnitTypes::Special_Right_Wall_Flame_Trap) {
				show_unit(u);
			}
		}

	}

	bool place_initial_unit(unit_t* u) {
		if (u->sprite->flags & SpriteFlags::Hidden) {
			// implement me
		}
		return true;
	}

	void add_completed_unit(int count, unit_t* u, bool increment_score) {
		if (u_hallucination(u)) return;
		if (ut_turret(u)) return;

		st.completed_unit_counts[u->owner][u->unit_type->id] += count;
		if (u->unit_type->staredit_group_flags & GroupFlags::Zerg) {
			st.supply_available[0][u->owner] += u->unit_type->supply_provided * count;
		} else if (u->unit_type->staredit_group_flags & GroupFlags::Terran) {
			st.supply_available[1][u->owner] += u->unit_type->supply_provided * count;
		} else if (u->unit_type->staredit_group_flags & GroupFlags::Protoss) {
			st.supply_available[2][u->owner] += u->unit_type->supply_provided * count;
		}

		if (u->unit_type->staredit_group_flags & GroupFlags::Factory) {
			st.completed_factory_counts[u->owner] += count;
		}
		if (u->unit_type->staredit_group_flags & GroupFlags::Men) {
			st.completed_building_counts[u->owner] += count;
		} else if (u->unit_type->staredit_group_flags & GroupFlags::Building) {
			st.completed_building_counts[u->owner] += count;
		}
		if (increment_score) {
			if (u->owner != 11) {
				if (u->unit_type->staredit_group_flags & GroupFlags::Men) {
					bool morphed = false;
					if (u->unit_type->id == UnitTypes::Zerg_Guardian) morphed = true;
					if (u->unit_type->id == UnitTypes::Zerg_Devourer) morphed = true;
					if (u->unit_type->id == UnitTypes::Protoss_Dark_Archon) morphed = true;
					if (u->unit_type->id == UnitTypes::Protoss_Archon) morphed = true;
					if (u->unit_type->id == UnitTypes::Zerg_Lurker) morphed = true;
					if (!morphed) st.total_non_buildings_ever_completed[u->owner] += count;
					st.unit_score[u->owner] += u->unit_type->build_score * count;
				} else if (u->unit_type->staredit_group_flags & GroupFlags::Building) {
					bool morphed = false;
					if (u->unit_type->id == UnitTypes::Zerg_Lair) morphed = true;
					if (u->unit_type->id == UnitTypes::Zerg_Hive) morphed = true;
					if (u->unit_type->id == UnitTypes::Zerg_Greater_Spire) morphed = true;
					if (u->unit_type->id == UnitTypes::Zerg_Spore_Colony) morphed = true;
					if (u->unit_type->id == UnitTypes::Zerg_Sunken_Colony) morphed = true;
					if (!morphed) st.total_buildings_ever_completed[u->owner] += count;
					st.building_score[u->owner] += u->unit_type->build_score * count;
				}
			}
		}

		if (st.completed_unit_counts[u->owner][u->unit_type->id] < 0) st.completed_unit_counts[u->owner][u->unit_type->id] = 0;

	}

	void remove_queued_order(unit_t* u, order_t* o) {
		if (o->order_type->highlight != -1) --u->order_queue_count;
		if (u->order_queue_count == -1) u->order_queue_count = 0;
		u->order_queue.remove(*o);
		bw_insert_list(st.free_orders, *o);
		--st.allocated_order_count;
	}

	void queue_order(unit_t* u, const order_type_t* order_type, order_t* insert_after, order_target target) {
		auto get_new = [&](const order_type_t* order_type, order_target target) {
			if (st.free_orders.empty()) return (order_t*)nullptr;
			order_t* o = &st.free_orders.front();
			st.free_orders.pop_front();
			++st.allocated_order_count;
			o->order_type = order_type;
			o->target = target;
			return o;
		};
		order_t* o = get_new(order_type, target);
		if (!o) {
			local_unit_status_error(u, 872);
			return;
		}
		if (o->order_type->highlight != -1) ++u->order_queue_count;
		if (insert_after) u->order_queue.insert(++u->order_queue.iterator_to(*insert_after), *o);
		else bw_insert_list(u->order_queue, *o);
	}

	void set_queued_order(unit_t* u, bool interrupt, const order_type_t* order_type, order_target target) {
		if (u->order_type->id == Orders::Die) return;
		while (!u->order_queue.empty()) {
			order_t* o = &u->order_queue.back();
			if (!o) break;
			if ((!interrupt || !o->order_type->can_be_interrupted) && o->order_type != order_type) break;
			remove_queued_order(u, o);
		}
		if (order_type->id == Orders::Cloak) {
			xcept("cloak fixme");
		} else {
			queue_order(u, order_type, nullptr, target);
		}
	}

	void iscript_run_to_idle(unit_t* u) {
		u->status_flags &= ~unit_t::status_flag_iscript_nobrk;
		u->sprite->flags &= ~sprite_t::flag_iscript_nobrk;
		iscript_unit_setter ius(this, u);
		unit_t* prev_iscript_order_unit = iscript_order_unit;
		iscript_order_unit = u;
		int anim = -1;
		switch (u->sprite->main_image->iscript_state.animation) {
		case iscript_anims::AirAttkInit:
		case iscript_anims::AirAttkRpt:
			anim = iscript_anims::AirAttkToIdle;
			break;
		case iscript_anims::AlmostBuilt:
			if (u->sprite->sprite_type->id != idenums::SPRITEID_SCV) break;
			if (u->sprite->sprite_type->id != idenums::SPRITEID_Drone) break;
			if (u->sprite->sprite_type->id != idenums::SPRITEID_Probe) break;
			anim = iscript_anims::GndAttkToIdle;
			break;
		case iscript_anims::GndAttkInit:
		case iscript_anims::GndAttkRpt:
			anim = iscript_anims::GndAttkToIdle;
			break;
		case iscript_anims::SpecialState1:
			if (u->sprite->sprite_type->id == idenums::SPRITEID_Medic) anim = iscript_anims::WalkingToIdle;
			break;
		case iscript_anims::CastSpell:
			anim = iscript_anims::WalkingToIdle;
			break;
		}
		if (anim != -1) {
			sprite_run_anim(u->sprite, anim);
		}
		u->movement_flags &= ~8;
		iscript_order_unit = prev_iscript_order_unit;
	}

	void activate_next_order(unit_t* u) {
		if (u->order_queue.empty()) return;
		if (u->ai) xcept("ai stuff");
		if ((u_in_building(u) || u_burrowed(u)) && u->order_queue.front().order_type->id != Orders::Die) return;
		const order_type_t* order_type = u->order_queue.front().order_type;
		order_target target = u->order_queue.front().target;
		remove_queued_order(u, &u->order_queue.front());

		u->user_action_flags &= ~1;
		u->status_flags &= ~(unit_t::status_flag_disabled | unit_t::status_flag_order_not_interruptible | unit_t::status_flag_holding_position);
		if (!order_type->can_be_interrupted) u->status_flags |= unit_t::status_flag_order_not_interruptible;
		u->order_queue_timer = 0;
		u->recent_order_timer = 0;

		u->order_type = order_type;
		u->order_state = 0;

		if (target.unit) {
			u->order_target.unit = target.unit;
			u->order_target.pos = target.unit->sprite->position;
			u->order_unit_type = nullptr;
		} else {
			u->order_target.unit = nullptr;
			u->order_target.pos = target.position;
			u->order_unit_type = target.unit_type;
		}
		if (!u->ai) u->auto_target_unit = nullptr;
		iscript_run_to_idle(u);
		if (!ut_turret(u) && u->subunit && ut_turret(u->subunit)) {
			const order_type_t* turret_order_type = order_type;
			if (order_type == u->unit_type->return_to_idle) turret_order_type = u->subunit->unit_type->return_to_idle;
			else if (order_type == u->unit_type->attack_unit) turret_order_type = u->subunit->unit_type->attack_unit;
			else if (order_type == u->unit_type->attack_move) turret_order_type = u->subunit->unit_type->attack_move;
			else if (!order_type->valid_for_turret) turret_order_type = nullptr;
			if (turret_order_type) {
				set_unit_order(u->subunit, turret_order_type, target);
			}
		}

	}

	void set_unit_order(unit_t* u, const order_type_t* order_type, order_target target = order_target()) {
		u->user_action_flags |= 1;
		set_queued_order(u, true, order_type, target);
		activate_next_order(u);
	}

	bool unit_finder_units_intersecting(unit_t* a, unit_t* b) const {
		if (a->unit_finder_bounding_box.to.x < b->unit_finder_bounding_box.from.x) return false;
		if (a->unit_finder_bounding_box.to.y < b->unit_finder_bounding_box.from.y) return false;
		if (a->unit_finder_bounding_box.from.x > b->unit_finder_bounding_box.to.x) return false;
		if (a->unit_finder_bounding_box.from.y > b->unit_finder_bounding_box.to.y) return false;
		return true;
	}

	bool unit_finder_unit_in_bounds(unit_t* u, rect bounds) const {
		if (u->unit_finder_bounding_box.to.x < bounds.from.x) return false;
		if (u->unit_finder_bounding_box.to.y < bounds.from.y) return false;
		if (u->unit_finder_bounding_box.from.x > bounds.to.x) return false;
		if (u->unit_finder_bounding_box.from.y > bounds.to.y) return false;
		return true;
	}

	void check_unit_collision(unit_t* u) {
		for (unit_t* nu : find_units(unit_sprite_bounding_box(u))) {
			if (u_grounded_building(nu)) u->status_flags |= unit_t::status_flag_collision;
			else if (!u_flying(nu) && (!u_gathering(nu) || u_grounded_building(u))) {
				if (unit_finder_units_intersecting(u, nu)) {
					nu->status_flags |= unit_t::status_flag_collision;
				}
			}
		}
	}

	void reset_unit_path(unit_t* u) {
		if (u->path) xcept("reset_unit_path: fixme");
	}

	void show_unit(unit_t* u) {
		if (!us_hidden(u)) return;
		u->sprite->flags &= ~sprite_t::flag_hidden;
		if (u->subunit && !ut_turret(u)) u->subunit->sprite->flags &= ~sprite_t::flag_hidden;
		refresh_unit_vision(u);
		update_unit_sprite(u);
		unit_finder_insert(u);
		if (u_grounded_building(u)) {
			xcept("update tiles (mask in flag_occupied)");
		}
		check_unit_collision(u);
		if (u_flying(u)) {
			xcept("set repulse angle");
		}
		reset_unit_path(u);
		
		u->movement_state = 0;
		if (u->sprite->elevation_level < 12) u->pathing_flags |= 1;
		else u->pathing_flags &= ~1;
		if (u->subunit && !ut_turret(u)) {
			reset_unit_path(u->subunit);
			u->subunit->movement_state = 0;
			if (u->subunit->sprite->elevation_level < 12) u->subunit->pathing_flags |= 1;
			else u->subunit->pathing_flags &= ~1;
		}
		st.hidden_units.remove(*u);
		bw_insert_list(st.visible_units, *u);
	}

	void complete_unit(unit_t* u) {

		if (ut_flyer(u)) {
			increment_unit_counts(u, -1);
			u->status_flags |= unit_t::status_flag_completed;
			increment_unit_counts(u, 1);
		} else {
			u->status_flags |= unit_t::status_flag_completed;
		}
		add_completed_unit(1, u, true);
		if (u->unit_type->id == UnitTypes::Spell_Scanner_Sweep || u->unit_type->id == UnitTypes::Special_Map_Revealer) {
			xcept("fixme scanner/map revealer");
		} else {
			if (us_hidden(u)) {
				if (u->unit_type->id != UnitTypes::Protoss_Interceptor && u->unit_type->id != UnitTypes::Protoss_Scarab) {
					show_unit(u);
				}
			}
		}
		bool is_trap = false;
		if (u->unit_type->id == UnitTypes::Special_Floor_Missile_Trap) is_trap = true;
		if (u->unit_type->id == UnitTypes::Special_Floor_Gun_Trap) is_trap = true;
		if (u->unit_type->id == UnitTypes::Special_Wall_Missile_Trap) is_trap = true;
		if (u->unit_type->id == UnitTypes::Special_Wall_Flame_Trap) is_trap = true;
		if (u->unit_type->id == UnitTypes::Special_Right_Wall_Missile_Trap) is_trap = true;
		if (u->unit_type->id == UnitTypes::Special_Right_Wall_Flame_Trap) is_trap = true;
		if (is_trap) {
			u->status_flags |= unit_t::status_flag_cloaked | unit_t::status_flag_requires_detector;
			u->visibility_flags = 0x80000000;
			u->secondary_order_timer = 0;
		}
		if (st.players[u->owner].controller == state::player_t::controller_rescue_passive) {
			xcept("fixme rescue passive");
		} else {
			if (st.players[u->owner].controller == state::player_t::controller_neutral) set_unit_order(u, get_order_type(Orders::Neutral));
			else if (st.players[u->owner].controller == state::player_t::controller_computer_game) set_unit_order(u, u->unit_type->computer_ai_idle);
			else set_unit_order(u, u->unit_type->human_ai_idle);
		}
		if (ut_flag(u, (unit_type_t::flags_t)0x800)) {
			xcept("fixme unknown flag");
		}
		u->air_strength = get_unit_strength(u, false);
		u->ground_strength = get_unit_strength(u, true);

	}

	unit_t* create_initial_unit(const unit_type_t* unit_type, xy pos, int owner) {
		unit_t* u = create_unit(unit_type, pos, owner);
		if (!u) {
			display_last_net_error_for_player(owner);
			return nullptr;
		}
		if (unit_type_spreads_creep(unit_type, true) || unit_type->flags&unit_type_t::flag_creep) {
			xcept("apply creep");
		}
		finish_building_unit(u);
		if (!place_initial_unit(u)) {
			xcept("place_initial_unit failed");
		}

		complete_unit(u);

		return u;
	}

	void display_last_net_error_for_player(int player) {
		log("fixme: display last error (%d)\n", st.last_net_error);
	}

};

void advance(state& st) {

	state_functions funcs(st);

	funcs.game_loop();
}

struct game_load_functions : state_functions {

	explicit game_load_functions(state& st) : state_functions(st) {}

	game_state& game_st = *st.game;

	unit_type_t* get_unit_type(int id) const {
		if ((size_t)id >= 228) xcept("invalid unit id %d", id);
		return &game_st.unit_types.vec[id];
	}
	const weapon_type_t* get_weapon_type(int id) const {
		if ((size_t)id >= 130) xcept("invalid weapon id %d", id);
		return &game_st.weapon_types.vec[id];
	}
	upgrade_type_t* get_upgrade_type(int id) const {
		if ((size_t)id >= 61) xcept("invalid upgrade id %d", id);
		return &game_st.upgrade_types.vec[id];
	}
	tech_type_t* get_tech_type(int id) const {
		if ((size_t)id >= 44) xcept("invalid tech id %d", id);
		return &game_st.tech_types.vec[id];
	}
	const flingy_type_t* get_flingy_type(int id) const {
		if ((size_t)id >= 209) xcept("invalid flingy id %d", id);
		return &global_st.flingy_types.vec[id];
	}

	void reset() {

		game_st.unit_types = data_loading::load_units_dat("arr\\units.dat");
		game_st.weapon_types = data_loading::load_weapons_dat("arr\\weapons.dat");
		game_st.upgrade_types = data_loading::load_upgrades_dat("arr\\upgrades.dat");
		game_st.tech_types = data_loading::load_techdata_dat("arr\\techdata.dat");

		auto fixup_unit_type = [&](unit_type_t*& ptr) {
			size_t index = (size_t)ptr;
			if (index == 228) ptr = nullptr;
			else ptr = get_unit_type(index);
		};
		auto fixup_weapon_type = [&](const weapon_type_t*& ptr) {
			size_t index = (size_t)ptr;
			if (index == 130) ptr = nullptr;
			else ptr = get_weapon_type(index);
		};
		auto fixup_upgrade_type = [&](upgrade_type_t*& ptr) {
			size_t index = (size_t)ptr;
			if (index == 61) ptr = nullptr;
			else ptr = get_upgrade_type(index);
		};
		auto fixup_flingy_type = [&](const flingy_type_t*& ptr) {
			size_t index = (size_t)ptr;
			ptr = get_flingy_type(index);
		};
		auto fixup_order_type = [&](const order_type_t*& ptr) {
			size_t index = (size_t)ptr;
			ptr = get_order_type(index);
		};
		auto fixup_image_type = [&](const image_type_t*& ptr) {
			size_t index = (size_t)ptr;
			if (index == 999) ptr = nullptr;
			else ptr = get_image_type(index);
		};

		for (auto& v : game_st.unit_types.vec) {
			fixup_flingy_type(v.flingy);
			fixup_unit_type(v.turret_unit_type);
			fixup_unit_type(v.subunit2);
			fixup_image_type(v.construction_animation);
			fixup_weapon_type(v.ground_weapon);
			fixup_weapon_type(v.air_weapon);
			fixup_upgrade_type(v.armor_upgrade);
			fixup_order_type(v.computer_ai_idle);
			fixup_order_type(v.human_ai_idle);
			fixup_order_type(v.return_to_idle);
			fixup_order_type(v.attack_unit);
			fixup_order_type(v.attack_move);
		}
		for (auto& v : game_st.weapon_types.vec) {
			fixup_flingy_type(v.flingy);
			fixup_upgrade_type(v.damage_upgrade);
		}

		for (auto&v : game_st.unit_type_allowed) v.fill(true);
		for (auto&v : game_st.tech_available) v.fill(true);
		st.tech_researched.fill({});
		for (auto&v : game_st.max_upgrade_levels) {
			for (size_t i = 0; i < game_st.max_upgrade_levels.size(); ++i) {
				v[i] = get_upgrade_type(i)->max_level;
			}
		}
		st.upgrade_levels.fill({});
		// upgrade progress?
		// UPRP stuff?

		for (auto&v : st.unit_counts) v.fill(0);
		for (auto&v : st.completed_unit_counts) v.fill(0);

		st.factory_counts.fill(0);
		st.building_counts.fill(0);
		st.non_building_counts.fill(0);

		st.completed_factory_counts.fill(0);
		st.completed_building_counts.fill(0);
		st.completed_non_building_counts.fill(0);

		st.total_buildings_ever_completed.fill(0);
		st.total_non_buildings_ever_completed.fill(0);

		st.unit_score.fill(0);
		st.building_score.fill(0);

		for (auto&v : st.supply_used) v.fill(0);
		for (auto&v : st.supply_available) v.fill(0);

		auto set_acquisition_ranges = [&]() {
			for (int i = 0; i < 228; ++i) {
				unit_type_t* unit_type = get_unit_type(i);
				const unit_type_t* attacking_type = unit_type->turret_unit_type ? unit_type->turret_unit_type : unit_type;
				const weapon_type_t* ground_weapon = attacking_type->ground_weapon;
				const weapon_type_t* air_weapon = attacking_type->air_weapon;
				int acq_range = attacking_type->target_acquisition_range;
				if (ground_weapon) acq_range = std::max(acq_range, ground_weapon->max_range);
				if (air_weapon) acq_range = std::max(acq_range, air_weapon->max_range);
				unit_type->target_acquisition_range = acq_range;
			}
		};
		set_acquisition_ranges();

		calculate_unit_strengths();

		generate_sight_values();

		load_tile_stuff();

		st.tiles.clear();
		st.tiles.resize(game_st.map_tile_width*game_st.map_tile_height);
		for (auto&v : st.tiles) {
			v.visible = 0xff;
			v.explored = 0xff;
		}
		st.tiles_mega_tile_index.clear();
		st.tiles_mega_tile_index.resize(st.tiles.size());

		st.gfx_creep_tiles.clear();
		st.gfx_creep_tiles.resize(game_st.map_tile_width * game_st.map_tile_height);

		st.order_timer_counter = 10;
		st.secondary_order_timer_counter = 150;

		st.visible_units.clear();
		st.hidden_units.clear();
		st.scanner_sweep_units.clear();
		st.sight_related_units.clear();
		for (auto&v : st.player_units) v.clear();

		auto clear_and_make_free = [&](auto& list, auto& free_list) {
			free_list.clear();
			memset(list.data(), 0, (char*)(list.data() + list.size()) - (char*)list.data());
			for (auto& v: list) {
				bw_insert_list(free_list, v);
			}
		};

		clear_and_make_free(st.units, st.free_units);
		clear_and_make_free(st.sprites, st.free_sprites);
		st.sprites_on_tile_line.clear();
		st.sprites_on_tile_line.resize(game_st.map_tile_height);
		clear_and_make_free(st.images, st.free_images);
		clear_and_make_free(st.orders, st.free_orders);
		st.allocated_order_count = 0;

		st.last_net_error = 0;

		game_st.is_replay = false;
		game_st.local_player = 0;

		size_t unit_finder_groups_size = (game_st.map_width + state::unit_finder_group_size - 1) / state::unit_finder_group_size;
		st.unit_finder_groups.resize(unit_finder_groups_size);
		for (auto& v : st.unit_finder_groups) {
			v.reserve(0x100);
		}

		int max_unit_width = 0;
		int max_unit_height = 0;
		for (auto&v : game_st.unit_types.vec) {
			int width = v.dimensions.from.x + 1 + v.dimensions.to.x;
			int height = v.dimensions.from.y + 1 + v.dimensions.to.y;
			if (width > max_unit_width) max_unit_width = width;
			if (height > max_unit_height) max_unit_height = height;
		}
		game_st.max_unit_width = max_unit_width;
		game_st.max_unit_height = max_unit_height;

		st.random_counts.fill(0);
		st.total_random_counts = 0;
		st.lcg_rand_state = 42;

	}

	void paths_create() {

		a_vector<uint8_t> unwalkable_flags(256 * 4 * 256 * 4);

		auto is_unwalkable = [&](size_t walk_x, size_t walk_y) {
			return unwalkable_flags[walk_y * 256 * 4 + walk_x] & 0x80 ? true : false;
		};
		auto is_walkable = [&](size_t walk_x, size_t walk_y) {
			return ~unwalkable_flags[walk_y * 256 * 4 + walk_x] & 0x80 ? true : false;
		};
		auto set_unwalkable = [&](size_t walk_x, size_t walk_y) {
			unwalkable_flags[walk_y * 256 * 4 + walk_x] |= 0x80;
		};
		auto set_walkable = [&](size_t walk_x, size_t walk_y) {
			unwalkable_flags[walk_y * 256 * 4 + walk_x] &= ~0x80;
		};
		auto is_dir_walkable = [&](size_t walk_x, size_t walk_y, int dir) {
			return ~unwalkable_flags[walk_y * 256 * 4 + walk_x] & (1 << dir) ? true : false;
		};
		auto is_dir_unwalkable = [&](size_t walk_x, size_t walk_y, int dir) {
			return unwalkable_flags[walk_y * 256 * 4 + walk_x] & (1 << dir) ? true : false;
		};
		auto set_dir_walkable = [&](size_t walk_x, size_t walk_y, int dir) {
			unwalkable_flags[walk_y * 256 * 4 + walk_x] &= ~(1 << dir);
		};
		auto set_dir_unwalkable = [&](size_t walk_x, size_t walk_y, int dir) {
			unwalkable_flags[walk_y * 256 * 4 + walk_x] |= 1 << dir;
		};
		auto flip_dir_walkable = [&](size_t walk_x, size_t walk_y, int dir) {
			unwalkable_flags[walk_y * 256 * 4 + walk_x] ^= 1 << dir;
		};
		auto is_every_dir_walkable = [&](size_t walk_x, size_t walk_y) {
			return unwalkable_flags[walk_y * 256 * 4 + walk_x] & 0x7f ? false : true;
		};
		auto is_any_dir_unwalkable = [&](size_t walk_x, size_t walk_y) {
			return unwalkable_flags[walk_y * 256 * 4 + walk_x] & 0x7f ? true : false;
		};
		auto get_unwalkable_dir_index = [&](size_t walk_x, size_t walk_y) {
			return unwalkable_flags[walk_y * 256 * 4 + walk_x] & 0xf;
		};

		auto set_unwalkable_flags = [&]() {

			for (size_t y = 0; y != game_st.map_tile_height; ++y) {
				for (size_t x = 0; x != game_st.map_tile_width; ++x) {
					uint16_t mega_tile_index = st.tiles_mega_tile_index[y * game_st.map_tile_width + x];

					auto& mt = game_st.vf4[mega_tile_index & 0x7fff];
					for (size_t sy = 0; sy < 4; ++sy) {
						for (size_t sx = 0; sx < 4; ++sx) {
							if (~mt.flags[sy * 4 + sx] & MiniTileFlags::Walkable) {
								set_unwalkable(x * 4 + sx, y * 4 + sy);
							}
						}
					}
				}
			}
			// Mark bottom part of map which is covered by the UI as unwalkable.
			if (game_st.map_walk_height >= 8) {
				for (size_t y = game_st.map_walk_height - 8; y != game_st.map_walk_height; ++y) {
					for (size_t x = 0; x != 20; ++x) {
						set_unwalkable(x, y);
					}
					if (game_st.map_walk_width >= 20) {
						for (size_t x = game_st.map_walk_width - 20; x != game_st.map_walk_width; ++x) {
							set_unwalkable(x, y);
						}
					}
					if (y >= game_st.map_walk_height - 4) {
						for (size_t x = 0; x != game_st.map_walk_width; ++x) {
							set_unwalkable(x, y);
						}
					}
				}
			}

			if (game_st.map_walk_width == 0 || game_st.map_walk_height == 0) xcept("map width/height is zero");

			for (size_t y = 0; y != game_st.map_walk_height; ++y) {
				for (size_t x = 0; x != game_st.map_walk_width; ++x) {
					if (is_unwalkable(x, y)) continue;
					if (y == 0 || is_unwalkable(x, y - 1)) set_dir_unwalkable(x, y, 0);
					if (x == game_st.map_walk_width - 1 || is_unwalkable(x + 1, y)) set_dir_unwalkable(x, y, 1);
					if (y == game_st.map_walk_height - 1 || is_unwalkable(x, y + 1)) set_dir_unwalkable(x, y, 2);
					if (x == 0 || is_unwalkable(x - 1, y)) set_dir_unwalkable(x, y, 3);
				}
			}
		};

		auto create_region = [&](rect_t<xy_t<size_t>> area) {
			auto* r = st.paths.get_new_region();
			uint16_t flags = (uint16_t)st.paths.tile_region_index[area.from.y * 256 + area.from.x];
			if (flags < 5000) xcept("attempt to create region inside another region");
			r->flags = flags;
			r->tile_area = area;
			r->tile_center.x = (area.from.x + area.to.x) / 2;
			r->tile_center.y = (area.from.y + area.to.y) / 2;
			size_t tile_count = 0;
			size_t index = r->index;
			for (size_t y = area.from.y; y != area.to.y; ++y) {
				for (size_t x = area.from.x; x != area.to.x; ++x) {
					if (st.paths.tile_region_index[y * 256 + x] < 5000) xcept("attempt to create overlapping region");
					st.paths.tile_region_index[y * 256 + x] = (uint16_t)index;
					++tile_count;
				}
			}
			r->tile_count = tile_count;
			return r;
		};

		auto create_unreachable_bottom_region = [&]() {
			auto* r = create_region({ {0, game_st.map_tile_height - 1}, {game_st.map_tile_width, game_st.map_tile_height} });
			// The scale of the area and center values are way off for this region.
			// It's probably a bug with no consequence, since the region is ignored when
			// creating the other regions.
			r->area = { {0, (int)game_st.map_height - 32}, {(int)game_st.map_width, (int)game_st.map_height} };
			r->center.x = ((fp8::integer(r->area.from.x) + fp8::integer(r->area.to.x)) / 2);
			r->center.y = ((fp8::integer(r->area.from.y) + fp8::integer(r->area.to.y)) / 2);
			r->flags = 0x1ffd;
			r->group_index = 0x4000;
		};

		auto create_regions = [&]() {

			size_t region_tile_index = 0;
			size_t region_x = 0;
			size_t region_y = 0;

			auto bb = st.paths.tile_bounding_box;

			auto find_empty_region = [&](size_t x, size_t y) {
				if (x >= bb.to.x) {
					x = bb.from.x;
					y = y + 1 >= bb.to.y ? bb.from.y : y + 1;
				}
				int start_x = x;
				int start_y = y;
				while (true) {
					size_t index = st.paths.tile_region_index[y * 256 + x];
					if (index >= 5000) {
						//log("found region at %d %d index %x\n", x, y, index);
						region_tile_index = index;
						region_x = x;
						region_y = y;
						return true;
					}
					++x;
					if (x >= bb.to.x) {
						x = bb.from.x;
						y = y + 1 >= bb.to.y ? bb.from.y : y + 1;
					}
					if (x == start_x && y == start_y) return false;
				}

			};

			size_t next_x = bb.from.x;
			size_t next_y = bb.from.y;

			bool has_expanded_all = false;
			size_t initial_regions_size = st.paths.regions.size();

			int prev_size = 7 * 8;

			while (true) {
				size_t start_x = next_x;
				size_t start_y = next_y;
				if (start_x >= bb.to.x) {
					start_x = bb.from.x;
					++start_y;
					if (start_y >= bb.to.y) start_y = bb.from.y;
				}
				if (find_empty_region(start_x, start_y)) {
						
					auto find_area = [this](size_t begin_x, size_t begin_y, size_t index) {
						size_t max_end_x = std::min(begin_x + 8, game_st.map_tile_width);
						size_t max_end_y = std::min(begin_y + 7, game_st.map_tile_height);

						size_t end_x = begin_x + 1;
						size_t end_y = begin_y + 1;
						bool x_is_good = true;
						bool y_is_good = true;
						bool its_all_good = true;
						while ((x_is_good || y_is_good) && (end_x != max_end_x && end_y != max_end_y)) {
							if (x_is_good) {
								for (size_t y = begin_y; y != end_y; ++y) {
									if (st.paths.tile_region_index[y * 256 + end_x] != index) {
										x_is_good = false;
										break;
									}
								}
							}
							if (y_is_good) {
								for (size_t x = begin_x; x != end_x; ++x) {
									if (st.paths.tile_region_index[end_y * 256 + x] != index) {
										y_is_good = false;
										break;
									}
								}
							}

							if (st.paths.tile_region_index[end_y * 256 + end_x] != index) {
								its_all_good = false;
							}
							if (its_all_good) {
								if (y_is_good) ++end_y;
								if (x_is_good) ++end_x;
							} else {
								if (y_is_good) ++end_y;
								else if (x_is_good) ++end_x;
							}
						}

						size_t width = end_x - begin_x;
						size_t height = end_y - begin_y;
						if (width > height * 3) width = height * 3;
						else if (height > width * 3) height = width * 3;
						end_x = begin_x + width;
						end_y = begin_y + height;

						return rect_t<xy_t<size_t>>{ {begin_x, begin_y}, {end_x, end_y} };
					};

					auto area = find_area(region_x, region_y, region_tile_index);

					int size = (area.to.x - area.from.x) * (area.to.y - area.from.y);
					if (size < prev_size) {
						auto best_area = area;
						int best_size = size;

						for (size_t n = 0; n != 25; ++n) {
							if (!find_empty_region(area.to.x, region_y)) break;
							area = find_area(region_x, region_y, region_tile_index);
							int size = (area.to.x - area.from.x) * (area.to.y - area.from.y);
							if (size > best_size) {
								best_size = size;
								best_area = area;
								if (size >= prev_size) break;
							}
						}

						area = best_area;
						size = best_size;
					}

					prev_size = size;

					next_x = area.to.x;
					next_y = area.from.y;

					if (st.paths.regions.size() >= 5000) xcept("too many regions (nooks and crannies)");

					auto* r = create_region(area);

					auto expand = [this](paths_t::region* r) {

						size_t& begin_x = r->tile_area.from.x;
						if (begin_x > 0) --begin_x;
						size_t& begin_y = r->tile_area.from.y;
						if (begin_y > 0) --begin_y;
						size_t& end_x = r->tile_area.to.x;
						if (end_x < (int)game_st.map_tile_width) ++end_x;
						size_t& end_y = r->tile_area.to.y;
						if (end_y < (int)game_st.map_tile_height) ++end_y;

						uint16_t flags = r->flags;
						size_t index = r->index;

						auto is_neighbor = [&](size_t x, size_t y) {
							if (x != 0 && st.paths.tile_region_index[y * 256 + x - 1] == index) return true;
							if (x != game_st.map_tile_width - 1 && st.paths.tile_region_index[y * 256 + x + 1] == index) return true;
							if (y != 0 && st.paths.tile_region_index[(y - 1) * 256 + x] == index) return true;
							if (y != game_st.map_tile_height - 1 && st.paths.tile_region_index[(y + 1) * 256 + x] == index) return true;
							return false;
						};

						for (int i = 0; i < 2; ++i) {
							for (size_t y = begin_y; y != end_y; ++y) {
								for (size_t x = begin_x; x != end_x; ++x) {
									if (st.paths.tile_region_index[y * 256 + x] == flags && is_neighbor(x, y)) {
										st.paths.tile_region_index[y * 256 + x] = index;
									}
								}
							}
						}

					};

					expand(r);

					if (size <= 6 && !has_expanded_all) {
						has_expanded_all = true;
						for (size_t i = initial_regions_size; i != st.paths.regions.size(); ++i) {
							expand(&st.paths.regions[i]);
						}
					}

				} else {
					if (st.paths.regions.size() >= 5000) xcept("too many regions (nooks and crannies)");
					log("created %d regions\n", st.paths.regions.size());
					break;
				}
			}

			auto get_neighbors = [&](size_t tile_x, size_t tile_y) {
				std::array<size_t, 8> r;
				size_t n = 0;
				auto test = [&](bool cond, size_t x, size_t y) {
					if (!cond) r[n++] = 0x1fff;
					else r[n++] = st.paths.tile_region_index[y * 256 + x];
				};
				test(tile_y > 0, tile_x, tile_y - 1);
				test(tile_x > 0, tile_x - 1, tile_y);
				test(tile_x + 1 < game_st.map_tile_width, tile_x + 1, tile_y);
				test(tile_y + 1 < game_st.map_tile_height, tile_x, tile_y + 1);
				test(tile_y > 0 && tile_x > 0, tile_x - 1, tile_y - 1);
				test(tile_y > 0 && tile_x + 1 < game_st.map_tile_width, tile_x + 1, tile_y - 1);
				test(tile_y + 1 < game_st.map_tile_height && tile_x > 0, tile_x - 1, tile_y + 1);
				test(tile_y + 1 < game_st.map_tile_height && tile_x + 1 < game_st.map_tile_width, tile_x + 1, tile_y + 1);
				return r;
			};

			auto refresh_regions = [&]() {

				for (auto* r : ptr(st.paths.regions)) {
					int max = std::numeric_limits<int>::max();
					int min = std::numeric_limits<int>::min();
					r->area = { { max, max }, { min, min } };
					r->tile_count = 0;
				}
				for (size_t y = 0; y != game_st.map_tile_height; ++y) {
					for (size_t x = 0; x != game_st.map_tile_width; ++x) {
						size_t index = st.paths.tile_region_index[y * 256 + x];
						if (index < 5000) {
							auto* r = &st.paths.regions[index];
							++r->tile_count;
							if (r->area.from.x >(int)x * 32) r->area.from.x = x * 32;
							if (r->area.from.y > (int)y * 32) r->area.from.y = y * 32;
							if (r->area.to.x < ((int)x + 1) * 32) r->area.to.x = (x + 1) * 32;
							if (r->area.to.y < ((int)y + 1) * 32) r->area.to.y = (y + 1) * 32;
						}
					}
				}

				for (auto* r : ptr(st.paths.regions)) {
					if (r->tile_count == 0) r->flags = 0x1fff;
				}

				for (auto* r : ptr(st.paths.regions)) {
					if (r->tile_count == 0) continue;

					r->walkable_neighbors.clear();
					r->non_walkable_neighbors.clear();

					for (int y = r->area.from.y / 32; y != r->area.to.y / 32; ++y) {
						for (int x = r->area.from.x / 32; x != r->area.to.x / 32; ++x) {
							if (st.paths.tile_region_index[y * 256 + x] != r->index) continue;
							auto neighbors = get_neighbors(x, y);
							for (size_t i = 0; i < 8; ++i) {
								size_t nindex = neighbors[i];
								if (nindex == 0x1fff || nindex == r->index) continue;
								auto* nr = &st.paths.regions[nindex];
								bool add = false;
								if (i < 4 || !r->walkable() || !nr->walkable()) {
									add = true;
								} else {
									auto is_2x2_walkable = [&](size_t walk_x, size_t walk_y) {
										if (!is_walkable(walk_x, walk_y)) return false;
										if (!is_walkable(walk_x + 1, walk_y)) return false;
										if (!is_walkable(walk_x, walk_y + 1)) return false;
										if (!is_walkable(walk_x + 1, walk_y + 1)) return false;
										return true;
									};


									size_t walk_x = x * 4;
									size_t walk_y = y * 4;
									if (i == 4) {
										if (is_2x2_walkable(walk_x - 2, walk_y - 2) && is_2x2_walkable(walk_x, walk_y)) {
											if (is_2x2_walkable(walk_x - 2, walk_y)) add = true;
											else if (is_2x2_walkable(walk_x, walk_y - 2)) add = true;
										}
									} else if (i == 5) {
										if (is_2x2_walkable(walk_x + 4, walk_y - 2) && is_2x2_walkable(walk_x + 2, walk_y)) {
											if (is_2x2_walkable(walk_x + 2, walk_y - 2)) add = true;
											else if (is_2x2_walkable(walk_x + 4, walk_y)) add = true;
										}
									} else if (i == 6) {
										if (is_2x2_walkable(walk_x, walk_y + 2) && is_2x2_walkable(walk_x - 2, walk_y + 4)) {
											if (is_2x2_walkable(walk_x - 2, walk_y + 2)) add = true;
											else if (is_2x2_walkable(walk_x, walk_y + 4)) add = true;
										}
									} else if (i == 7) {
										if (is_2x2_walkable(walk_x + 2, walk_y + 2) && is_2x2_walkable(walk_x + 4, walk_y + 4)) {
											if (is_2x2_walkable(walk_x + 4, walk_y + 2)) add = true;
											else if (is_2x2_walkable(walk_x + 2, walk_y + 4)) add = true;
										}
									}
								}
								if (add) {
									if (nr->walkable()) {
										if (std::find(r->walkable_neighbors.begin(), r->walkable_neighbors.end(), nr) == r->walkable_neighbors.end()) {
											r->walkable_neighbors.push_back(nr);
										}
									} else {
										if (std::find(r->non_walkable_neighbors.begin(), r->non_walkable_neighbors.end(), nr) == r->non_walkable_neighbors.end()) {
											r->non_walkable_neighbors.push_back(nr);
										}
									}
								}
							}
						}
					}

					if (!r->non_walkable_neighbors.empty()) {
						for (auto& v : r->non_walkable_neighbors) {
							if (v == &st.paths.regions.front() && &v != &r->non_walkable_neighbors.back()) std::swap(v, r->non_walkable_neighbors.back());
						}
					}

				}

				for (auto* r : ptr(st.paths.regions)) {
					r->center.x = ((fp8::integer(r->area.from.x) + fp8::integer(r->area.to.x)) / 2);
					r->center.y = ((fp8::integer(r->area.from.y) + fp8::integer(r->area.to.y)) / 2);
				}

				for (auto* r : ptr(st.paths.regions)) {
					if (r->group_index < 0x4000) r->group_index = 0;
				}
				a_vector<paths_t::region*> stack;
				size_t next_group_index = 1;
				for (auto* r : ptr(st.paths.regions)) {
					if (r->group_index == 0 && r->tile_count) {
						size_t group_index = next_group_index++;
						r->group_index = group_index;
						stack.push_back(r);
						while (!stack.empty()) {
							auto* cr = stack.back();
							stack.pop_back();
							for (auto* nr : (cr->walkable() ? cr->walkable_neighbors : cr->non_walkable_neighbors)) {
								if (nr->group_index == 0) {
									nr->group_index = group_index;
									stack.push_back(nr);
								}
							}
						}
					}
				}

			};

			refresh_regions();

			for (size_t n = 6;; n += 2) {

				for (auto* r : reverse(ptr(st.paths.regions))) {
					if (r->tile_count == 0 || r->tile_count >= n || r->group_index >= 0x4000) continue;
					paths_t::region* smallest_neighbor = nullptr;
					auto eval = [&](auto* nr) {
						if (nr->tile_count == 0 || nr->group_index >= 0x4000 || nr->flags != r->flags) return;
						if (!smallest_neighbor || nr->tile_count < smallest_neighbor->tile_count) {
							smallest_neighbor = nr;
						}
					};
					for (auto* nr : r->walkable_neighbors) eval(nr);
					for (auto* nr : r->non_walkable_neighbors) eval(nr);
					if (smallest_neighbor) {
						auto* merge_into = smallest_neighbor;
						for (size_t y = r->area.from.y / 32; y != r->area.to.y / 32; ++y) {
							for (size_t x = r->area.from.x / 32; x != r->area.to.x / 32; ++x) {
								size_t& index = st.paths.tile_region_index[y * 256 + x];
								if (index == r->index) index = merge_into->index;
							}
						}
						merge_into->tile_count += r->tile_count;
						r->tile_count = 0;
						r->flags = 0x1fff;
						if (r->area.from.x < merge_into->area.from.x) merge_into->area.from.x = r->area.from.x;
						if (r->area.from.y < merge_into->area.from.y) merge_into->area.from.y = r->area.from.y;
						if (r->area.to.x > merge_into->area.to.x) merge_into->area.to.x = r->area.to.x;
						if (r->area.to.y > merge_into->area.to.y) merge_into->area.to.y = r->area.to.y;
					}
				}

				size_t n_non_empty_regions = 0;
				for (auto* r : ptr(st.paths.regions)) {
					if (r->tile_count) ++n_non_empty_regions;
				}
				log("n_non_empty_regions is %d\n", n_non_empty_regions);
				if (n_non_empty_regions < 2500) break;
			}

 			a_vector<size_t> reindex(5000);
 			size_t new_region_count = 0;
			for (size_t i = 0; i != st.paths.regions.size(); ++i) {
				auto* r = &st.paths.regions[i];
				r->walkable_neighbors.clear();
				r->non_walkable_neighbors.clear();
				if (r->tile_count == 0) continue;
				size_t new_index = new_region_count++;
				reindex[i] = new_index;
				r->index = new_index;
				st.paths.regions[new_index] = std::move(*r);
			}
			for (size_t y = 0; y != game_st.map_tile_height; ++y) {
				for (size_t x = 0; x != game_st.map_tile_height; ++x) {
					size_t& index = st.paths.tile_region_index[y * 256 + x];
					index = reindex[index];
				}
			}
			st.paths.regions.resize(new_region_count);

			log("new_region_count is %d\n", new_region_count);

			refresh_regions();

			for (size_t y = 0; y != game_st.map_tile_height; ++y) {
				for (size_t x = 0; x != game_st.map_tile_width; ++x) {
					auto tile = st.tiles[y * game_st.map_tile_width + x];
					if (~tile.flags & tile_t::flag_partially_walkable) continue;
					auto neighbors = get_neighbors(x, y);
					auto* r = &st.paths.regions[st.paths.tile_region_index[y * 256 + x]];
					auto count_4x1_walkable = [&](size_t walk_x, size_t walk_y) {
						size_t r = 0;
						if (is_walkable(walk_x, walk_y)) ++r;
						if (is_walkable(walk_x + 1, walk_y)) ++r;
						if (is_walkable(walk_x + 2, walk_y)) ++r;
						if (is_walkable(walk_x + 3, walk_y)) ++r;
						return r;
					};
					auto count_1x4_walkable = [&](size_t walk_x, size_t walk_y) {
						size_t r = 0;
						if (is_walkable(walk_x, walk_y)) ++r;
						if (is_walkable(walk_x, walk_y + 1)) ++r;
						if (is_walkable(walk_x, walk_y + 2)) ++r;
						if (is_walkable(walk_x, walk_y + 3)) ++r;
						return r;
					};
					size_t walk_x = x * 4;
					size_t walk_y = y * 4;
					paths_t::region* r2 = nullptr;
					if (!r->walkable()) {
						std::array<size_t, 4> n_walkable {};
						n_walkable[0] = count_4x1_walkable(walk_x, walk_y);
						n_walkable[1] = count_1x4_walkable(walk_x, walk_y);
						n_walkable[2] = count_1x4_walkable(walk_x + 3, walk_y);
						n_walkable[3] = count_4x1_walkable(walk_x, walk_y + 3);
						size_t highest_n = 0;
						size_t highest_nindex;
						for (size_t i = 4; i != 0;) {
							--i;
							size_t n = n_walkable[i];
							if (n <= highest_n) continue;
							size_t nindex = neighbors[i];
							if (nindex == r->index) continue;
							if (nindex < 0x2000) {
								if (nindex >= 5000) continue;
								auto* nr = &st.paths.regions[nindex];
								if (nr->walkable()) {
									highest_n = n;
									highest_nindex = nindex;
								}
							} else {
								bool set = false;
								if (i == 0 && count_4x1_walkable(walk_x, walk_y - 1)) set = true;
								if (i == 1 && count_1x4_walkable(walk_x - 1, walk_y)) set = true;
								if (i == 2 && count_1x4_walkable(walk_x + 4, walk_y)) set = true;
								if (i == 3 && count_4x1_walkable(walk_x, walk_y + 4)) set = true;
								if (set) {
									highest_n = n;
									highest_nindex = nindex;
								}
							}
						}
						if (highest_n) {
							if (highest_nindex < 0x2000) r2 = &st.paths.regions[highest_nindex];
							else r2 = st.paths.split_regions[highest_nindex - 0x2000].a;
						}
					} else {
						std::array<size_t, 8> n_unwalkable {};
						n_unwalkable[0] = 4 - count_4x1_walkable(walk_x, walk_y);
						n_unwalkable[1] = 4 - count_1x4_walkable(walk_x, walk_y);
						n_unwalkable[2] = 4 - count_1x4_walkable(walk_x + 3, walk_y);
						n_unwalkable[3] = 4 - count_4x1_walkable(walk_x, walk_y + 3);
						n_unwalkable[4] = is_walkable(walk_x, walk_y) ? 0 : 1;
						n_unwalkable[5] = is_walkable(walk_x + 3, walk_y) ? 0 : 1;
						n_unwalkable[6] = is_walkable(walk_x, walk_y + 3) ? 0 : 1;
						n_unwalkable[7] = is_walkable(walk_x + 3, walk_y + 3) ? 0 : 1;
						size_t highest_n = 0;
						size_t highest_nindex;
						for (size_t i = 8; i != 0;) {
							--i;
							size_t n = n_unwalkable[i];
							if (n <= highest_n) continue;
							size_t nindex = neighbors[i];
							if (nindex == r->index) continue;
							if (nindex < 0x2000) {
								if (nindex >= 5000) continue;
								auto* nr = &st.paths.regions[nindex];
								if (nr->walkable()) {
									highest_n = n;
									highest_nindex = nindex;
								}
							} else {
								bool set = false;
								if (i == 0 && count_4x1_walkable(walk_x, walk_y - 1) == 0) set = true;
								if (i == 1 && count_1x4_walkable(walk_x - 1, walk_y) == 0) set = true;
								if (i == 2 && count_1x4_walkable(walk_x + 4, walk_y) == 0) set = true;
								if (i == 3 && count_4x1_walkable(walk_x, walk_y + 4) == 0) set = true;
								if (i == 4 && !is_walkable(walk_x - 1, walk_y - 1)) set = true;
								if (i == 5 && !is_walkable(walk_x + 4, walk_y - 1)) set = true;
								if (i == 6 && !is_walkable(walk_x - 1, walk_y + 4)) set = true;
								if (i == 7 && !is_walkable(walk_x + 4, walk_y + 4)) set = true;
								if (set) {
									highest_n = n;
									highest_nindex = nindex;
								}
							}
						}
						if (highest_n) {
							if (highest_nindex < 0x2000) r2 = &st.paths.regions[highest_nindex];
							else r2 = st.paths.split_regions[highest_nindex - 0x2000].a;
						}
					}
					uint16_t mask = 0;
					if (!r->walkable() && (!r2 || !r2->walkable())) {
						mask = 0xffff;
					}
					st.paths.tile_region_index[y * 256 + x] = 0x2000 + st.paths.split_regions.size();
					st.paths.split_regions.push_back({ mask, r, r2 ? r2 : r });
				}
			}
			log("created %d split regions\n", st.paths.split_regions.size());

			for (auto* r : ptr(st.paths.regions)) {
				r->priority = 0;
				static_vector<paths_t::region*, 5> nvec;
				for (auto* nr : r->non_walkable_neighbors) {
					if (nr->tile_count >= 4) {
						if (std::find(nvec.begin(), nvec.end(), nr) == nvec.end()) {
							nvec.push_back(nr);
							if (nvec.size() >= 5) break;
						}
					}
				}
				if (nvec.size() >= 2) r->priority = nvec.size();
			}

		};

		auto create_contours = [&]() {

			size_t next_x = 0;
			size_t next_y = 0;
			auto next = [&]() {
				size_t x = next_x;
				size_t y = next_y;
				if (x >= game_st.map_walk_width) {
					xcept("create_contours::next: unreachable");
					x = 0;
					++y;
					if (y >= game_st.map_walk_height) y = 0;
				}
				x &= ~(4 - 1);
				size_t start_x = x;
				size_t start_y = y;
				while (is_every_dir_walkable(x, y) && is_every_dir_walkable(x + 1, y) && is_every_dir_walkable(x + 2, y) && is_every_dir_walkable(x + 3, y)) {
					x += 4;
					if (x == game_st.map_walk_width) {
						x = 0;
						++y;
						if (y == game_st.map_walk_height) {
							y = 0;
						}
					}
					if (x == start_x && y == start_y) return false;
				}
				while (!is_walkable(x, y) || is_every_dir_walkable(x, y)) {
					++x;
					if (x == game_st.map_walk_width) xcept("create_contours: out of bounds");
				}
				next_x = x;
				next_y = y;
				return true;
			};

			while (next()) {

				std::array<int, 16> clut = { -1,  -1, 8, -1, 8, 8, -1, 8, 0, -1, 8, 0, 7, 8, -1, 7 };
				std::array<int, 16> nlut = { 8, -1, 8, 8, -1, 8, -1, -1, 7, -1, 8, 7, 0, 8, -1, 0 };

				size_t x = next_x;
				size_t y = next_y;

				if (is_dir_walkable(x, y, 0) && is_dir_walkable(x, y, 1) && is_dir_unwalkable(x, y, 2)) {
					++y;
					--x;
				}
				size_t first_unwalkable_dir = 0;
				int lut1val = is_dir_unwalkable(x, y, 0) && is_dir_unwalkable(x, y, 3) ? 0 : 1;
				for (int i = 0; i < 4; ++i) {
					if (is_dir_unwalkable(x, y, i)) {
						first_unwalkable_dir = i;
						break;
					}
				}

				size_t clut_index = first_unwalkable_dir + 4 * lut1val;
				int start_cx = clut[clut_index * 2] + x * 8;
				int start_cy = clut[clut_index * 2 + 1] + y * 8;

				flip_dir_walkable(x, y, first_unwalkable_dir);

				int cx = start_cx;
				int cy = start_cy;

				auto cur_dir = first_unwalkable_dir;
				while (true) {
					auto next_dir = (cur_dir + 1) & 3;
					int relx = next_dir == 1 ? 1 : next_dir == 3 ? -1 : 0;
					int rely = next_dir == 0 ? -1 : next_dir == 2 ? 1 : 0;
					flip_dir_walkable(x, y, cur_dir);
					int next_walkable = 0;
					if (is_dir_walkable(x, y, next_dir)) {
						next_walkable = 1;
						while (is_dir_unwalkable(x + relx, y + rely, cur_dir)) {
							x += relx;
							y += rely;
							flip_dir_walkable(x, y, cur_dir);
							if (is_dir_unwalkable(x, y, next_dir)) {
								next_walkable = 0;
								break;
							}
						}
					}
					size_t nlut_index = cur_dir + 4 * next_walkable;

					int nx = nlut[nlut_index * 2] + x * 8;
					int ny = nlut[nlut_index * 2 + 1] + y * 8;

					uint8_t flags0 = (uint8_t)(cur_dir ^ 2 * lut1val ^ ~(2 * cur_dir) & 3);
					uint8_t flags1 = (uint8_t)(cur_dir ^ 2 * (next_walkable ^ cur_dir & 1) ^ 1);
					if (cur_dir == 0) { 
						uint8_t flags = flags0 & 3 | 4 * (flags1 & 3 | 4 * (lut1val | 2 * next_walkable));
						st.paths.contours[cur_dir].push_back({ {cy, cx, nx}, cur_dir, flags });
					} else if (cur_dir == 1) {
						uint8_t flags = flags0 & 3 | 4 * (flags1 & 3 | 4 * (lut1val | 2 * next_walkable));
						st.paths.contours[cur_dir].push_back({ { cx, cy, ny}, cur_dir, flags });
					} else if (cur_dir == 2) {
						uint8_t flags = flags1 & 3 | 4 * (flags0 & 3 | 4 * (next_walkable | 2 * lut1val));
						st.paths.contours[cur_dir].push_back({ { cy, nx, cx}, cur_dir, flags });
					} else if (cur_dir == 3) {
						uint8_t flags = flags1 & 3 | 4 * (flags0 & 3 | 4 * (next_walkable | 2 * lut1val));
						st.paths.contours[cur_dir].push_back({ { cx, ny, cy}, cur_dir, flags });
					} else xcept("unreachable");

					if (!next_walkable) cur_dir = next_dir;
					else {
						int nrel[4][2] = { { 1, -1 }, { 1, 1 }, { -1, 1 }, { -1, -1 } };
						x += nrel[cur_dir][0];
						y += nrel[cur_dir][1];
						if (cur_dir == 0) cur_dir = 3;
						else --cur_dir;
					}
					cx = nx;
					cy = ny;
					lut1val = next_walkable;

					if (cx == start_cx && cy == start_cy) break;
				}

				flip_dir_walkable(x, y, cur_dir);

			}

			for (auto& v : st.paths.contours) {
				std::sort(v.begin(), v.end(), [&](auto& a, auto& b) {
					if (a.v[0] != b.v[0]) return a.v[0] < b.v[0];
					if (a.v[1] != b.v[1]) return a.v[1] < b.v[1];
					xcept("unreachable: two equal contours");
					return false;
				});
			}

		};

		st.paths.tile_bounding_box = { {0, 0}, {game_st.map_tile_width, game_st.map_tile_height} };

		set_unwalkable_flags();

// 		for (size_t y = 0; y != game_st.map_tile_height; ++y) {
// 			for (size_t x = 0; x != game_st.map_tile_width; ++x) {
// 				auto& index = st.paths.tile_region_index[y * 256 + x];
// 				index = 4999;
// 			}
// 		}

		for (size_t y = 0; y != game_st.map_tile_height; ++y) {
			for (size_t x = 0; x != game_st.map_tile_width; ++x) {
				auto& index = st.paths.tile_region_index[y * 256 + x];
				auto& t = st.tiles[y * game_st.map_tile_height + x];
				if (~t.flags & tile_t::flag_walkable) index = 0x1ffd;
				else if (t.flags & tile_t::flag_middle) index = 0x1ff9;
				else if (t.flags & tile_t::flag_high) index = 0x1ffa;
				else index = 0x1ffb;
			}
		}


		create_unreachable_bottom_region();

		create_regions();

		create_contours();

	}

	int get_unit_strength(const unit_type_t* unit_type, const weapon_type_t* weapon_type) {
		switch (unit_type->id) {
		case UnitTypes::Terran_Vulture_Spider_Mine:
		case UnitTypes::Protoss_Interceptor:
		case UnitTypes::Protoss_Scarab:
			return 0;
		}
		int hp = unit_type->hitpoints.integer_part();
		if (unit_type->has_shield) hp += unit_type->shield_points;
		if (hp == 0) return 0;
		int fact = weapon_type->damage_factor;
		int cd = weapon_type->cooldown;
		int dmg = weapon_type->damage_amount;
		int range = weapon_type->max_range;
		unsigned int a = (range / (unsigned)cd) * fact * dmg;
		unsigned int b = (hp * ((int64_t)(fact*dmg << 11) / cd)) >> 8;
		// This function calculates (int)(sqrt(x)*7.58)
		auto sqrt_x_times_7_58 = [&](int x) {
			if (x <= 0) return 0;
			int value = 1;
			while (true) {
				int f_eval = value * value;
				int f_derivative = 2 * value;
				int delta = (f_eval - x + f_derivative - 1) / f_derivative;
				if (delta == 0) break;
				while (std::numeric_limits<int>::max() / (value - delta) < (value - delta)) {
					delta /= 2;
				}
				value -= delta;
			}
			value = value * 758 / 100;
			size_t n = 8;
			while (n > 0) {
				int nv = value + n / 2 + 1;
				int r = (int)(((int64_t)nv * nv * 10000) / (758 * 758));
				if (r < x) {
					value = nv;
					n -= n / 2 + 1;
				} else n /= 2;
			}
			return value;
		};
		int score = sqrt_x_times_7_58(a + b);
		switch (unit_type->id) {
		case UnitTypes::Terran_SCV:
		case UnitTypes::Zerg_Drone:
		case UnitTypes::Protoss_Probe:
			return score / 4;
		case UnitTypes::Terran_Firebat:
		case UnitTypes::Zerg_Mutalisk:
		case UnitTypes::Protoss_Zealot:
			return score * 2;
		case UnitTypes::Zerg_Scourge:
		case UnitTypes::Zerg_Infested_Terran:
			return score / 16;
		case UnitTypes::Protoss_Reaver:
			return score / 10;
		default:
			return score;
		}
	};

	void calculate_unit_strengths() {

		for (int idx = 0; idx < 228; ++idx) {

			const unit_type_t* unit_type = get_unit_type(idx);
			const unit_type_t* attacking_type = unit_type;
			int air_strength = 0;
			int ground_strength = 0;
			if (attacking_type->id != UnitTypes::Zerg_Larva && attacking_type->id != UnitTypes::Zerg_Egg && attacking_type->id != UnitTypes::Zerg_Cocoon && attacking_type->id != UnitTypes::Zerg_Lurker_Egg) {
				if (attacking_type->id == UnitTypes::Protoss_Carrier || attacking_type->id == UnitTypes::Hero_Gantrithor) attacking_type = get_unit_type(UnitTypes::Protoss_Interceptor);
				else if (attacking_type->id == UnitTypes::Protoss_Reaver || attacking_type->id == UnitTypes::Hero_Warbringer) attacking_type = get_unit_type(UnitTypes::Protoss_Scarab);
				else if (attacking_type->turret_unit_type) attacking_type = attacking_type->turret_unit_type;

				const weapon_type_t* air_weapon = attacking_type->air_weapon;
				if (!air_weapon) air_strength = 1;
				else air_strength = get_unit_strength(unit_type, air_weapon);

				const weapon_type_t* ground_weapon = attacking_type->ground_weapon;
				if (!ground_weapon) ground_strength = 1;
				else ground_strength = get_unit_strength(unit_type, ground_weapon);
			}
			if (air_strength == 1 && ground_strength > air_strength) air_strength = 0;
			if (ground_strength == 1 && air_strength > ground_strength) ground_strength = 0;

			//log("strengths for %d is %d %d\n", idx, air_strength, ground_strength);

			game_st.unit_air_strength[idx] = air_strength;
			game_st.unit_ground_strength[idx] = ground_strength;

		}

	}

	void generate_sight_values() {
		for (size_t i = 0; i < game_st.sight_values.size(); ++i) {
			auto&v = game_st.sight_values[i];
			v.max_width = 3 + (int)i * 2;
			v.max_height = 3 + (int)i * 2;
			v.min_width = 3;
			v.min_height = 3;
			v.min_mask_size = 0;
			v.ext_masked_count = 0;
		}

		for (auto&v : game_st.sight_values) {
			struct base_mask_t {
				sight_values_t::maskdat_node_t*maskdat_node;
				bool masked;
			};
			a_vector<base_mask_t> base_mask(v.max_width*v.max_height);
			auto mask = [&](size_t index) {
				if (index >= base_mask.size()) xcept("attempt to mask invalid base mask index %d (size %d) (broken brood war algorithm)", index, base_mask.size());
				base_mask[index].masked = true;
			};
			v.min_mask_size = v.min_width*v.min_height;
			int offx = v.max_width / 2 - v.min_width / 2;
			int offy = v.max_height / 2 - v.min_height / 2;
			for (int y = 0; y < v.min_height; ++y) {
				for (int x = 0; x < v.min_width; ++x) {
					mask((offy + y)*v.max_width + offx + x);
				}
			}
			auto generate_base_mask = [&]() {
				int offset = v.max_height / 2 - v.max_width / 2;
				int half_width = v.max_width / 2;
				int	max_x2 = half_width;
				int max_x1 = half_width * 2;
				int cur_x1 = 0;
				int cur_x2 = half_width;
				int i = 0;
				int max_i = half_width;
				int cursize1 = 0;
				int cursize2 = half_width*half_width;
				int min_cursize2 = half_width * (half_width - 1);
				int min_cursize2_chg = half_width * 2;
				while (true) {
					if (cur_x1 <= max_x1) {
						for (int i = 0; i <= max_x1 - cur_x1; ++i) {
							mask((offset + cur_x2)*v.max_width + cur_x1 + i);
							mask((offset + max_x2)*v.max_width + cur_x1 + i);
						}
					}
					if (cur_x2 <= max_x2) {
						for (int i = 0; i <= max_x2 - cur_x2; ++i) {
							mask((offset + cur_x1)*v.max_width + cur_x2 + i);
							mask((offset + max_x1)*v.max_width + cur_x2 + i);
						}
					}
					cursize2 += 1 - cursize1 - 2;
					cursize1 += 2;
					--cur_x2;
					++max_x2;
					if (cursize2 <= min_cursize2) {
						--max_i;
						++cur_x1;
						--max_x1;
						min_cursize2 -= min_cursize2_chg - 2;
						min_cursize2_chg -= 2;
					}

					++i;
					if (i > max_i) break;
				}
			};
			generate_base_mask();
			int masked_count = 0;
			for (auto&v : base_mask) {
				if (v.masked) ++masked_count;
			}
			log("%d %d - masked_count is %d\n", v.max_width, v.max_height, masked_count);

			v.ext_masked_count = masked_count - v.min_mask_size;
			v.maskdat.clear();
			v.maskdat.resize(masked_count);

			auto*center = &base_mask[v.max_height / 2 * v.max_width + v.max_width / 2];
			center->maskdat_node = &v.maskdat.front();

			auto at = [&](int index) -> base_mask_t& {
				auto*r = &center[index];
				if (r < base_mask.data() || r >= base_mask.data() + base_mask.size()) xcept("attempt to access invalid base mask center-relative index %d (size %d)", index, base_mask.size());
				return *r;
			};

			size_t next_entry_index = 1;

			int cur_x = -1;
			int cur_y = -1;
			int added_count = 1;
			for (int i = 2; added_count < masked_count; i += 2) {
				for (int dir = 0; dir < 4; ++dir) {
					static const std::array<int, 4> direction_x = { 1,0,-1,0 };
					static const std::array<int, 4> direction_y = { 0,1,0,-1 };
					int this_x;
					int this_y;
					auto do_n = [&](int n) {
						for (int i = 0; i < n; ++i) {
							if (at(this_y*v.max_width + this_x).masked) {
								if (this_x || this_y) {
									auto*this_entry = &v.maskdat.at(next_entry_index++);

									int prev_x = this_x;
									int prev_y = this_y;
									if (prev_x > 0) --prev_x;
									else if (prev_x < 0) ++prev_x;
									if (prev_y > 0) --prev_y;
									else if (prev_y < 0) ++prev_y;
									if (std::abs(prev_x) == std::abs(prev_y) || (this_x == 0 && direction_x[dir]) || (this_y == 0 && direction_y[dir])) {
										this_entry->prev = this_entry->prev2 = at(prev_y * v.max_width + prev_x).maskdat_node;
										this_entry->prev_count = 1;
									} else {
										this_entry->prev = at(prev_y * v.max_width + prev_x).maskdat_node;
										int prev2_x = prev_x;
										int prev2_y = prev_y;
										if (std::abs(prev2_x) <= std::abs(prev2_y)) {
											if (this_x >= 0) ++prev2_x;
											else --prev2_x;
										} else {
											if (this_y >= 0) ++prev2_y;
											else --prev2_y;
										}
										this_entry->prev2 = at(prev2_y * v.max_width + prev2_x).maskdat_node;
										this_entry->prev_count = 2;
									}
									this_entry->map_index_offset = this_y * game_st.map_tile_width + this_x;
									this_entry->x = this_x;
									this_entry->y = this_y;
									at(this_y * v.max_width + this_x).maskdat_node = this_entry;
									++added_count;
								}
							}
							this_x += direction_x[dir];
							this_y += direction_y[dir];
						}
					};
					const std::array<int, 4> max_i = { v.max_height,v.max_width,v.max_height,v.max_width };
					if (i > max_i[dir]) {
						this_x = cur_x + i * direction_x[dir];
						this_y = cur_y + i * direction_y[dir];
						do_n(1);
					} else {
						this_x = cur_x + direction_x[dir];
						this_y = cur_y + direction_y[dir];
						do_n(std::min(max_i[(dir + 1) % 4] - 1, i));
					}
					cur_x = this_x - direction_x[dir];
					cur_y = this_y - direction_y[dir];
				}
				if (i < v.max_width - 1) --cur_x;
				if (i < v.max_height - 1) --cur_y;
			}

		}

		// 	log("generated sight stuff:\n");
		// 	for (auto&v : game_st.sight_values) {
		// 		log("%d %d %d %d %d %d\n", v.max_width, v.max_height, v.min_width, v.max_height, v.min_mask_size, v.ext_masked_count);
		// 		size_t maskdat_size = v.min_mask_size + v.ext_masked_count;
		// 		log(" maskdat: \n");
		// 		//log("%s", hexdump(v.maskdat.data(), maskdat_size * 20));
		// 		for (auto&v : v.maskdat) {
		// 			log("%d %d %d\n", v.map_index_offset, v.x, v.y);
		// 		}
		// 	}

	}

	void load_tile_stuff() {
		std::array<const char*, 8> tileset_names = {
			"badlands", "platform", "install", "AshWorld", "Jungle", "Desert", "Ice", "Twilight"
		};

		auto set_mega_tile_flags = [&]() {
			game_st.mega_tile_flags.resize(game_st.vf4.size());
			for (size_t i = 0; i < game_st.mega_tile_flags.size(); ++i) {
				int flags = 0;
				auto& mt = game_st.vf4[i];
				int walkable_count = 0;
				int middle_count = 0;
				int high_count = 0;
				int very_high_count = 0;
				for (size_t y = 0; y < 4; ++y) {
					for (size_t x = 0; x < 4; ++x) {
						if (mt.flags[y * 4 + x] & MiniTileFlags::Walkable) ++walkable_count;
						if (mt.flags[y * 4 + x] & MiniTileFlags::Middle) ++middle_count;
						if (mt.flags[y * 4 + x] & MiniTileFlags::High) ++high_count;
						if (mt.flags[y * 4 + x] & MiniTileFlags::BlocksView) ++very_high_count;
					}
				}
				if (walkable_count > 12) flags |= tile_t::flag_walkable;
				else flags |= tile_t::flag_unwalkable;
				if (walkable_count && walkable_count != 0x10) flags |= tile_t::flag_partially_walkable;
				if (high_count < 12 && middle_count + high_count >= 12) flags |= tile_t::flag_middle;
				if (high_count >= 12) flags |= tile_t::flag_high;
				if (very_high_count) flags |= tile_t::flag_very_high;
				game_st.mega_tile_flags[i] = flags;
			}

		};

		load_data_file(game_st.vf4, format("Tileset\\%s.vf4", tileset_names.at(game_st.tileset_index)));
		set_mega_tile_flags();
		load_data_file(game_st.cv5, format("Tileset\\%s.cv5", tileset_names.at(game_st.tileset_index)));
	}

	struct tag_t {
		tag_t() = default;
		tag_t(const char str[4]) : data({ str[0], str[1], str[2], str[3] }) {}
		tag_t(const std::array<char, 4> data) : data(data) {}
		std::array<char, 4> data = {};
		bool operator==(const tag_t&n) const {
			return data == n.data;
		}
		size_t operator()(const tag_t&v) const {
			return std::hash<uint32_t>()((uint32_t)v.data[0] | v.data[1] << 8 | v.data[2] << 16 | v.data[3] << 24);
		}
	};

	void load_map_file(a_string filename) {

		// campaign stuff? see load_map_file

		log("load map file '%s'\n", filename);

		SArchive archive(filename);
		a_vector<uint8_t> data;
		load_data_file(data, "staredit\\scenario.chk");

		using data_loading::data_reader_le;

		a_unordered_map<tag_t, std::function<void(data_reader_le)>, tag_t> tag_funcs;

		auto tagstr = [&](tag_t tag) {
			return a_string(tag.data.data(), 4);
		};

		using tag_list_t = a_vector<std::pair<tag_t, bool>>;
		auto read_chunks = [&](const tag_list_t&tags) {
			data_reader_le r(data.data(), data.data() + data.size());
			a_unordered_map<tag_t, data_reader_le> chunks;
			while (r.left()) {
				tag_t tag = r.get<std::array<char, 4>>();
				uint32_t len = r.get<uint32_t>();
				//log("tag '%.4s' len %d\n", (char*)&tag, len);
				uint8_t*chunk_data = r.ptr;
				r.skip(len);
				chunks[tag] = { chunk_data, r.ptr };
			}
			for (auto&v : tags) {
				tag_t tag = std::get<0>(v);
				auto i = chunks.find(tag);
				if (i == chunks.end()) {
					if (std::get<1>(v)) xcept("map is missing required chunk '%s'", tagstr(tag));
				} else {
					if (!tag_funcs[tag]) xcept("tag '%s' is missing a function", tagstr(tag));
					log("loading tag '%s'...\n", tagstr(tag));
					tag_funcs[tag](i->second);
				}
			}
		};

		int version = 0;
		tag_funcs["VER "] = [&](data_reader_le r) {
			version = r.get<uint16_t>();
			log("VER: version is %d\n", version);
		};
		tag_funcs["DIM "] = [&](data_reader_le r) {
			game_st.map_tile_width = r.get<uint16_t>();
			game_st.map_tile_height = r.get<uint16_t>();
			game_st.map_walk_width = game_st.map_tile_width * 4;
			game_st.map_walk_height = game_st.map_tile_width * 4;
			game_st.map_width = game_st.map_tile_width * 32;
			game_st.map_height = game_st.map_tile_height * 32;
			log("DIM: dimensions are %d %d\n", game_st.map_tile_width, game_st.map_tile_height);
		};
		tag_funcs["ERA "] = [&](data_reader_le r) {
			game_st.tileset_index = r.get<uint16_t>() % 8;
			log("ERA: tileset is %d\n", game_st.tileset_index);
		};
		tag_funcs["OWNR"] = [&](data_reader_le r) {
			for (size_t i = 0; i < 12; ++i) {
				st.players[i].controller = r.get<int8_t>();
				if (st.players[i].controller == state::player_t::controller_open) st.players[i].controller = state::player_t::controller_occupied;
				if (st.players[i].controller == state::player_t::controller_computer) st.players[i].controller = state::player_t::controller_computer_game;
			}
		};
		tag_funcs["SIDE"] = [&](data_reader_le r) {
			for (size_t i = 0; i < 12; ++i) {
				st.players[i].race = r.get<int8_t>();
			}
		};
		tag_funcs["STR "] = [&](data_reader_le r) {
			auto start = r;
			size_t num = r.get<uint16_t>();
			game_st.map_strings.clear();
			game_st.map_strings.resize(num);
			for (size_t i = 0; i < num; ++i) {
				size_t offset = r.get<uint16_t>();
				auto t = start;
				t.skip(offset);
				char*b = (char*)t.ptr;
				while (t.get<char>());
				game_st.map_strings[i] = a_string(b, (char*)t.ptr - b - 1);
				//log("string %d: %s\n", i, game_st.map_strings[i]);
			}
		};
		tag_funcs["SPRP"] = [&](data_reader_le r) {
			game_st.scenario_name = game_st.get_string(r.get<uint16_t>());
			game_st.scenario_description = game_st.get_string(r.get<uint16_t>());
			log("SPRP: scenario name: '%s',  description: '%s'\n", game_st.scenario_name, game_st.scenario_description);
		};
		tag_funcs["FORC"] = [&](data_reader_le r) {
			for (size_t i = 0; i < 12; ++i) st.players[i].force = 0;
			for (size_t i = 0; i < 4; ++i) {
				game_st.forces[i].name = "";
				game_st.forces[i].flags = 0;
			}
			if (r.left()) {
				for (size_t i = 0; i < 8; ++i) {
					st.players[i].force = r.get<uint8_t>();
				}
				for (size_t i = 0; i < 4; ++i) {
					game_st.forces[i].name = game_st.get_string(r.get<uint16_t>());
				}
				for (size_t i = 0; i < 4; ++i) {
					game_st.forces[i].flags = r.get<uint8_t>();
				}
			}
		};
		tag_funcs["VCOD"] = [&](data_reader_le r) {
			// Starcraft does some verification/checksum stuff here
		};


		tag_funcs["MTXM"] = [&](data_reader_le r) {
			auto gfx_tiles_data = r.get_vec<uint16_t>(game_st.map_tile_width * game_st.map_tile_height);
			game_st.gfx_tiles.resize(gfx_tiles_data.size());
			for (size_t i = 0; i < gfx_tiles_data.size(); ++i) {
				game_st.gfx_tiles[i] = tile_id(gfx_tiles_data[i]);
			}
			for (size_t i = 0; i < game_st.gfx_tiles.size(); ++i) {
				tile_id tile_id = game_st.gfx_tiles[i];
				size_t megatile_index = game_st.cv5.at(tile_id.group_index()).megaTileRef[tile_id.subtile_index()];
				int cv5_flags = game_st.cv5.at(tile_id.group_index()).flags & ~(tile_t::flag_walkable | tile_t::flag_unwalkable | tile_t::flag_very_high | tile_t::flag_middle | tile_t::flag_high | tile_t::flag_partially_walkable);
				st.tiles_mega_tile_index[i] = (uint16_t)megatile_index;
				st.tiles[i].flags = game_st.mega_tile_flags.at(megatile_index) | cv5_flags;
				if (tile_id.has_creep()) {
					st.tiles_mega_tile_index[i] |= 0x8000;
					st.tiles[i].flags |= tile_t::flag_has_creep;
				}
			}

			tiles_flags_and(0, game_st.map_tile_height - 2, 5, 1, ~(tile_t::flag_walkable | tile_t::flag_has_creep | tile_t::flag_partially_walkable));
			tiles_flags_or(0, game_st.map_tile_height - 2, 5, 1, tile_t::flag_unbuildable);
			tiles_flags_and(game_st.map_tile_width - 5, game_st.map_tile_height - 2, 5, 1, ~(tile_t::flag_walkable | tile_t::flag_has_creep | tile_t::flag_partially_walkable));
			tiles_flags_or(game_st.map_tile_width - 5, game_st.map_tile_height - 2, 5, 1, tile_t::flag_unbuildable);

			tiles_flags_and(0, game_st.map_tile_height - 1, game_st.map_tile_width, 1, ~(tile_t::flag_walkable | tile_t::flag_has_creep | tile_t::flag_partially_walkable));
			tiles_flags_or(0, game_st.map_tile_height - 1, game_st.map_tile_width, 1, tile_t::flag_unbuildable);

			paths_create();
		};

		bool bVictoryCondition = false;
		bool bStartingUnits = false;
		bool bTournamentModeEnabled = false;
		bool bAlliesEnabled = true;

		tag_funcs["THG2"] = [&](data_reader_le r) {
			while (r.left()) {
				int unit_type = r.get<uint16_t>();
				int x = r.get<uint16_t>();
				int y = r.get<uint16_t>();
				int owner = r.get<uint8_t>();
				r.get<uint8_t>();
				r.get<uint8_t>();
				int flags = r.get<uint8_t>();
				if (flags & 0x10) {
					xcept("create thingy of type %d", unit_type);
				} else {
					if (unit_type == UnitTypes::Special_Upper_Level_Door) owner = 11;
					if (unit_type == UnitTypes::Special_Right_Upper_Level_Door) owner = 11;
					if (unit_type == UnitTypes::Special_Pit_Door) owner = 11;
					if (unit_type == UnitTypes::Special_Right_Pit_Door) owner = 11;
					if ((!bVictoryCondition && !bStartingUnits && !bTournamentModeEnabled) || owner == 11) {
						xcept("create (thingy) unit of type %d", unit_type);
						if (flags & 0x80) xcept("disable thingy unit");
					}
				}
			}
		};
		tag_funcs["MASK"] = [&](data_reader_le r) {
			auto mask = r.get_vec<uint8_t>(game_st.map_tile_width*game_st.map_tile_height);
			for (size_t i = 0; i < mask.size(); ++i) {
				st.tiles[i].visible |= mask[i];
				st.tiles[i].explored |= mask[i];
			}
		};

		auto units = [&](data_reader_le r, bool broodwar) {
			auto uses_default_settings = r.get_vec<uint8_t>(228);
			auto hp = r.get_vec<uint32_t>(228);
			auto shield_points = r.get_vec<uint16_t>(228);
			auto armor = r.get_vec<uint8_t>(228);
			auto build_time = r.get_vec<uint16_t>(228);
			auto mineral_cost = r.get_vec<uint16_t>(228);
			auto gas_cost = r.get_vec<uint16_t>(228);
			auto string_index = r.get_vec<uint16_t>(228);
			auto weapon_damage = r.get_vec<uint16_t>(broodwar ? 130 : 100);
			auto weapon_bonus_damage = r.get_vec<uint16_t>(broodwar ? 130 : 100);
			for (int i = 0; i < 228; ++i) {
				if (uses_default_settings[i]) continue;
				unit_type_t* unit_type = get_unit_type(i);
				unit_type->hitpoints = fp8::from_raw(hp[i]);
				unit_type->shield_points = shield_points[i];
				unit_type->armor = armor[i];
				unit_type->build_time = build_time[i];
				unit_type->mineral_cost = mineral_cost[i];
				unit_type->gas_cost = gas_cost[i];
				unit_type->unit_map_string_index = string_index[i];
				const unit_type_t* attacking_type = unit_type->turret_unit_type ? unit_type->turret_unit_type : unit_type;
				weapon_type_t* ground_weapon = &game_st.weapon_types.vec.at(attacking_type->ground_weapon->id);
				weapon_type_t* air_weapon = &game_st.weapon_types.vec.at(attacking_type->air_weapon->id);
				if (ground_weapon) {
					ground_weapon->damage_amount = weapon_damage[ground_weapon->id];
					ground_weapon->damage_bonus =  weapon_bonus_damage[ground_weapon->id];
				}
				if (air_weapon) {
					air_weapon->damage_amount = weapon_damage[air_weapon->id];
					air_weapon->damage_bonus = weapon_bonus_damage[air_weapon->id];
				}
			}
		};

		auto upgrades = [&](data_reader_le r, bool broodwar) {
			auto uses_default_settings = r.get_vec<uint8_t>(broodwar ? 62 : 46);
			auto mineral_cost = r.get_vec<uint16_t>(broodwar ? 61 : 46);
			auto mineral_cost_factor = r.get_vec<uint16_t>(broodwar ? 61 : 46);
			auto gas_cost = r.get_vec<uint16_t>(broodwar ? 61 : 46);
			auto gas_cost_factor = r.get_vec<uint16_t>(broodwar ? 61 : 46);
			auto research_time = r.get_vec<uint16_t>(broodwar ? 61 : 46);
			auto research_time_factor = r.get_vec<uint16_t>(broodwar ? 61 : 46);
			for (int i = 0; i < (broodwar ? 61 : 46); ++i) {
				if (uses_default_settings[i]) continue;
				upgrade_type_t*upg = get_upgrade_type(i);
				upg->mineral_cost_base = mineral_cost[i];
				upg->mineral_cost_factor = mineral_cost_factor[i];
				upg->gas_cost_base = gas_cost[i];
				upg->gas_cost_factor = gas_cost_factor[i];
				upg->research_time_base = research_time[i];
				upg->research_time_factor = research_time_factor[i];
			}
		};

		auto techdata = [&](data_reader_le r, bool broodwar) {
			auto uses_default_settings = r.get_vec<uint8_t>(broodwar ? 44 : 24);
			auto mineral_cost = r.get_vec<uint16_t>(broodwar ? 44 : 24);
			auto gas_cost = r.get_vec<uint16_t>(broodwar ? 44 : 24);
			auto build_time = r.get_vec<uint16_t>(broodwar ? 44 : 24);
			auto energy_cost = r.get_vec<uint16_t>(broodwar ? 44 : 24);
			for (int i = 0; i < (broodwar ? 44 : 24); ++i) {
				if (uses_default_settings[i]) continue;
				tech_type_t*tech = get_tech_type(i);
				tech->mineral_cost = mineral_cost[i];
				tech->gas_cost = gas_cost[i];
				tech->research_time = build_time[i];
				tech->energy_cost = energy_cost[i];
			}
		};

		auto upgrade_restrictions = [&](data_reader_le r, bool broodwar) {
			int count = broodwar ? 61 : 46;
			auto player_max_level = r.get_vec<uint8_t>(12 * count);
			auto player_cur_level = r.get_vec<uint8_t>(12 * count);
			auto global_max_level = r.get_vec<uint8_t>(count);
			auto global_cur_level = r.get_vec<uint8_t>(count);
			auto player_uses_global_default = r.get_vec<uint8_t>(12 * count);
			for (int player = 0; player < 12; ++player) {
				for (int upgrade = 0; upgrade < count; ++upgrade) {
					game_st.max_upgrade_levels[player][upgrade] = !!player_uses_global_default[player*count + upgrade] ? global_max_level[upgrade] : player_max_level[player*count + upgrade];
					st.upgrade_levels[player][upgrade] = !!player_uses_global_default[player*count + upgrade] ? global_cur_level[upgrade] : player_cur_level[player*count + upgrade];
				}
			}
		};
		auto tech_restrictions = [&](data_reader_le r, bool broodwar) {
			int count = broodwar ? 44 : 24;
			auto player_available = r.get_vec<uint8_t>(12 * count);
			auto player_researched = r.get_vec<uint8_t>(12 * count);
			auto global_available = r.get_vec<uint8_t>(count);
			auto global_researched = r.get_vec<uint8_t>(count);
			auto player_uses_global_default = r.get_vec<uint8_t>(12 * count);
			for (int player = 0; player < 12; ++player) {
				for (int upgrade = 0; upgrade < count; ++upgrade) {
					game_st.tech_available[player][upgrade] = !!(!!player_uses_global_default[player*count + upgrade] ? global_available[upgrade] : player_available[player*count + upgrade]);
					st.tech_researched[player][upgrade] = !!(!!player_uses_global_default[player*count + upgrade] ? global_researched[upgrade] : player_researched[player*count + upgrade]);
				}
			}
		};

		tag_funcs["UNIS"] = [&](data_reader_le r) {
			if (bVictoryCondition || bStartingUnits || bTournamentModeEnabled) xcept("wrong game mode");
			units(r, false);
		};
		tag_funcs["UPGS"] = [&](data_reader_le r) {
			if (bVictoryCondition || bStartingUnits || bTournamentModeEnabled) xcept("wrong game mode");
			upgrades(r, false);
		};
		tag_funcs["TECS"] = [&](data_reader_le r) {
			if (bVictoryCondition || bStartingUnits || bTournamentModeEnabled) xcept("wrong game mode");
			techdata(r, false);
		};
		tag_funcs["PUNI"] = [&](data_reader_le r) {
			if (bVictoryCondition || bStartingUnits || bTournamentModeEnabled) xcept("wrong game mode");
			auto player_available = r.get_vec<std::array<uint8_t, 228>>(12);
			auto global_available = r.get_vec<uint8_t>(228);
			auto player_uses_global_default = r.get_vec<std::array<uint8_t, 228>>(12);
			for (int player = 0; player < 12; ++player) {
				for (int unit = 0; unit < 228; ++unit) {
					game_st.unit_type_allowed[player][unit] = !!(!!player_uses_global_default[player][unit] ? global_available[unit] : player_available[player][unit]);
				}
			}
		};
		tag_funcs["UPGR"] = [&](data_reader_le r) {
			if (bVictoryCondition || bStartingUnits || bTournamentModeEnabled) xcept("wrong game mode");
			upgrade_restrictions(r, false);
		};
		tag_funcs["PTEC"] = [&](data_reader_le r) {
			if (bVictoryCondition || bStartingUnits || bTournamentModeEnabled) xcept("wrong game mode");
			tech_restrictions(r, false);
		};

		tag_funcs["UNIx"] = [&](data_reader_le r) {
			if (bVictoryCondition || bStartingUnits || bTournamentModeEnabled) xcept("wrong game mode");
			units(r, true);
		};
		tag_funcs["UPGx"] = [&](data_reader_le r) {
			if (bVictoryCondition || bStartingUnits || bTournamentModeEnabled) xcept("wrong game mode");
			upgrades(r, true);
		};
		tag_funcs["TECx"] = [&](data_reader_le r) {
			if (bVictoryCondition || bStartingUnits || bTournamentModeEnabled) xcept("wrong game mode");
			techdata(r, true);
		};
		tag_funcs["PUPx"] = [&](data_reader_le r) {
			if (bVictoryCondition || bStartingUnits || bTournamentModeEnabled) xcept("wrong game mode");
			upgrade_restrictions(r, true);
		};
		tag_funcs["PTEx"] = [&](data_reader_le r) {
			if (bVictoryCondition || bStartingUnits || bTournamentModeEnabled) xcept("wrong game mode");
			tech_restrictions(r, true);
		};

		tag_funcs["UNIT"] = [&](data_reader_le r) {
			while (r.left()) {

				int id = r.get<uint32_t>();
				int x = r.get<uint16_t>();
				int y = r.get<uint16_t>();
				int unit_type_id = r.get<uint16_t>();
				int link = r.get<uint16_t>();
				int valid_flags = r.get<uint16_t>();
				int valid_properties = r.get<uint16_t>();
				int owner = r.get<uint8_t>();
				int hp_percent = r.get<uint8_t>();
				int shield_percent = r.get<uint8_t>();
				int energy_percent = r.get<uint8_t>();
				int resources = r.get<uint32_t>();
				int units_in_hangar = r.get<uint16_t>();
				int flags = r.get<uint16_t>();
				r.get<uint32_t>();
				int related_unit_id = r.get<uint32_t>();

				if ((size_t)unit_type_id >= 228) xcept("UNIT: invalid unit type %d", unit_type_id);
				if ((size_t)owner >= 12) xcept("UNIT: invalid owner %d", owner);

				const unit_type_t*unit_type = get_unit_type(unit_type_id);

				log("create unit of type %d\n", unit_type->id);

				if (unit_type->id == UnitTypes::Special_Start_Location) {
					game_st.start_locations[owner] = { x, y };
					// 				int local_player = 0;
					// 				if (owner == local_player) {
					// 					int move_screen_to_tile_x = x / 32 >= 10 ? x / 32 - 10 : 0;
					// 					int move_screen_to_tile_y = y / 32 >= 6 ? y / 32 - 6 : 0;
					// 				}
					continue;
				}
				auto should_create_units_for_this_player = [&]() {
					if (owner >= 8) return true;
					int controller = st.players[owner].controller;
					if (controller == state::player_t::controller_computer_game) return true;
					if (controller == state::player_t::controller_occupied) return true;
					if (controller == state::player_t::controller_rescue_passive) return true;
					if (controller == state::player_t::controller_unused_rescue_active) return true;
					return false;
				};
				auto is_neutral_unit = [&]() {
					if (owner == 11) return true;
					if (unit_type->id == UnitTypes::Resource_Mineral_Field) return true;
					if (unit_type->id == UnitTypes::Resource_Mineral_Field_Type_2) return true;
					if (unit_type->id == UnitTypes::Resource_Mineral_Field_Type_3) return true;
					if (unit_type->id == UnitTypes::Resource_Vespene_Geyser) return true;
					if (unit_type->id == UnitTypes::Critter_Rhynadon) return true;
					if (unit_type->id == UnitTypes::Critter_Bengalaas) return true;
					if (unit_type->id == UnitTypes::Critter_Scantid) return true;
					if (unit_type->id == UnitTypes::Critter_Kakaru) return true;
					if (unit_type->id == UnitTypes::Critter_Ragnasaur) return true;
					if (unit_type->id == UnitTypes::Critter_Ursadon) return true;
					return false;
				};
				if (!should_create_units_for_this_player()) continue;
				if (bStartingUnits && !is_neutral_unit()) continue;
				if (!bVictoryCondition && !bStartingUnits && !bTournamentModeEnabled) {
					// what is player_force?
					std::array<int, 12> player_force{};
					if (player_force[owner] && ~unit_type->staredit_group_flags & GroupFlags::Neutral) continue;
				}

				unit_t* u = create_initial_unit(unit_type, { x,y }, owner);

				if (valid_properties & 0x2) set_unit_hp(u, std::max(fp8::truncate(u->unit_type->hitpoints * hp_percent / 100), fp8::integer(1) / 256));
				if (valid_properties & 0x4) set_unit_shield_points(u, fp8::integer(u->unit_type->shield_points * shield_percent / 100));
				if (valid_properties & 0x8) set_unit_energy(u, fp8::truncate(unit_max_energy(u) * energy_percent / 100));
				if (valid_properties & 0x10) set_unit_resources(u, resources);
				// more stuff...
				
				log("created initial unit %p with id %d\n", u, u - st.units.data());

			}
		};

		tag_funcs["UPRP"] = [&](data_reader_le r) {
			for (int i = 0; i < 64; ++i) {
				int valid_flags = r.get<uint16_t>();
				int valid_properties = r.get<uint16_t>();
				int owner = r.get<uint8_t>();
				int hp_percent = r.get<uint8_t>();
				int shield_percent = r.get<uint8_t>();
				int energy_percent = r.get<uint8_t>();
				int resources = r.get<uint32_t>();
				int units_in_hangar = r.get<uint16_t>();
				int flags = r.get<uint16_t>();
				r.get<uint32_t>();
			}
		};

		tag_funcs["MRGN"] = [&](data_reader_le r) {
			// 64 or 256 entries
			while (r.left()) {
				int left = r.get<int32_t>();
				int top = r.get<int32_t>();
				int right = r.get<int32_t>();
				int bottom = r.get<int32_t>();
				a_string name = game_st.get_string(r.get<uint16_t>());
				int elevation_flags = r.get<uint16_t>();
			}
		};

		tag_funcs["TRIG"] = [&](data_reader_le r) {
			// todo
			while (r.left()) {
				r.skip(2400);
			}
		};

		// This doesn't really belong here, but it can stay until we have proper
		// game setup code
		st.local_mask = 1;

		for (int i = 0; i < 12; ++i) {
			st.alliances[i].fill(0);
			st.alliances[i][i] = 1;
		}

		for (int i = 0; i < 12; ++i) {
			st.alliances[i][11] = 1;
			st.alliances[11][i] = 1;

			if (bAlliesEnabled && !bTournamentModeEnabled) {
				for (int i2 = 0; i2 < 12; ++i2) {
					if (st.players[i].controller == state::player_t::controller_computer_game && st.players[i2].controller == state::player_t::controller_computer_game) {
						st.alliances[i][i2] = 2;
					}
				}
			}
		}

		for (int i = 0; i < 12; ++i) {
			st.shared_vision[i] = 1 << i;
			if (st.players[i].controller == state::player_t::controller_rescue_passive || st.players[i].controller == state::player_t::controller_neutral) {
				for (int i2 = 0; i2 < 12; ++i2) {
					st.alliances[i][i2] = 1;
					st.alliances[i2][i] = 1;
				}
			}
		}

		if (!bVictoryCondition && !bStartingUnits && !bTournamentModeEnabled) {

		}

		allow_random = true;

		read_chunks({
			{"VER ", true},
			{"DIM ", true},
			{"ERA ", true},
			{"OWNR", true},
			{"SIDE", true},
			{"STR ", true},
			{"SPRP", true},
			{"FORC", true},
			{"VCOD", true}
		});

		reset();

		if (version == 59) {

			// todo: check game mode
			// this is for use map settings
			tag_list_t tags = {
				{"STR ", true},
				{"MTXM", true},
				{"THG2", true},
				{"MASK", true},
				{"UNIS", true},
				{"UPGS", true},
				{"TECS", true},
				{"PUNI", true},
				{"UPGR", true},
				{"PTEC", true},
				{"UNIx", false},
				{"UPGx", false},
				{"TECx", false},
				{"PUPx", false},
				{"PTEx", false},
				{"UNIT", true},
				{"UPRP", true},
				{"MRGN", true},
				{"TRIG", true}
			};
			read_chunks(tags);

		} else xcept("unsupported map version %d", version);

		allow_random = false;

	}
};

void global_init(global_state&st) {

	auto get_sprite_type = [&](int id) {
		if ((size_t)id >= 517) xcept("invalid sprite id %d", id);
		return &st.sprite_types.vec[id];
	};
	auto get_image_type = [&](int id) {
		if ((size_t)id >= 999) xcept("invalid image id %d", id);
		return &st.image_types.vec[id];
	};

	auto load_iscript_bin = [&]() {

		using namespace iscript_opcodes;
		std::array<const char*, 69> ins_data;

		ins_data[opc_playfram] = "2";
		ins_data[opc_playframtile] = "2";
		ins_data[opc_sethorpos] = "s1";
		ins_data[opc_setvertpos] = "s1";
		ins_data[opc_setpos] = "s1s1";
		ins_data[opc_wait] = "1";
		ins_data[opc_waitrand] = "11";
		ins_data[opc_goto] = "j";
		ins_data[opc_imgol] = "211";
		ins_data[opc_imgul] = "211";
		ins_data[opc_imgolorig] = "2";
		ins_data[opc_switchul] = "2";
		ins_data[opc___0c] = "";
		ins_data[opc_imgoluselo] = "211";
		ins_data[opc_imguluselo] = "211";
		ins_data[opc_sprol] = "211";
		ins_data[opc_highsprol] = "211";
		ins_data[opc_lowsprul] = "211";
		ins_data[opc_uflunstable] = "2";
		ins_data[opc_spruluselo] = "211";
		ins_data[opc_sprul] = "211";
		ins_data[opc_sproluselo] = "21";
		ins_data[opc_end] = "e";
		ins_data[opc_setflipstate] = "1";
		ins_data[opc_playsnd] = "2";
		ins_data[opc_playsndrand] = "v";
		ins_data[opc_playsndbtwn] = "22";
		ins_data[opc_domissiledmg] = "";
		ins_data[opc_attackmelee] = "v";
		ins_data[opc_followmaingraphic] = "";
		ins_data[opc_randcondjmp] = "1b";
		ins_data[opc_turnccwise] = "1";
		ins_data[opc_turncwise] = "1";
		ins_data[opc_turn1cwise] = "";
		ins_data[opc_turnrand] = "1";
		ins_data[opc_setspawnframe] = "1";
		ins_data[opc_sigorder] = "1";
		ins_data[opc_attackwith] = "1";
		ins_data[opc_attack] = "";
		ins_data[opc_castspell] = "";
		ins_data[opc_useweapon] = "1";
		ins_data[opc_move] = "1";
		ins_data[opc_gotorepeatattk] = "";
		ins_data[opc_engframe] = "1";
		ins_data[opc_engset] = "1";
		ins_data[opc___2d] = "";
		ins_data[opc_nobrkcodestart] = "";
		ins_data[opc_nobrkcodeend] = "";
		ins_data[opc_ignorerest] = "";
		ins_data[opc_attkshiftproj] = "1";
		ins_data[opc_tmprmgraphicstart] = "";
		ins_data[opc_tmprmgraphicend] = "";
		ins_data[opc_setfldirect] = "1";
		ins_data[opc_call] = "b";
		ins_data[opc_return] = "";
		ins_data[opc_setflspeed] = "2";
		ins_data[opc_creategasoverlays] = "1";
		ins_data[opc_pwrupcondjmp] = "b";
		ins_data[opc_trgtrangecondjmp] = "2b";
		ins_data[opc_trgtarccondjmp] = "22b";
		ins_data[opc_curdirectcondjmp] = "22b";
		ins_data[opc_imgulnextid] = "11";
		ins_data[opc___3e] = "";
		ins_data[opc_liftoffcondjmp] = "b";
		ins_data[opc_warpoverlay] = "2";
		ins_data[opc_orderdone] = "1";
		ins_data[opc_grdsprol] = "211";
		ins_data[opc___43] = "";
		ins_data[opc_dogrddamage] = "";

		a_unordered_map<int, a_vector<size_t>> animation_pc;
		a_vector<int> program_data;

		program_data.push_back(0); // invalid/null pc

		using data_loading::data_reader_le;

		a_vector<uint8_t> data;
		load_data_file(data, "scripts\\iscript.bin");
		data_reader_le base_r(data.data(), data.data() + data.size());
		auto r = base_r;
		size_t id_list_offset = r.get<uint32_t>();
		r.skip(id_list_offset);
		while (r.left()) {
			int id = r.get<int16_t>();
			if (id == -1) break;
			size_t script_address = r.get<uint16_t>();
			//log("loading script %d at %d\n", id, script_address);
			auto script_r = base_r;
			script_r.skip(script_address);
			auto signature = script_r.get<std::array<char, 4>>();
			//auto script_program_r = script_r;

			a_unordered_map<int, size_t> decode_map;

			auto decode_at = [&](size_t initial_address) {
				a_deque<std::tuple<size_t, size_t>> branches;
				std::function<size_t(int)> decode = [&](size_t initial_address) {
					if (!initial_address) xcept("iscript load: attempt to decode instruction at null address");
					auto in = decode_map.emplace(initial_address, 0);
					if (!in.second) {
						//log("instruction at 0x%04x already exists with index %d\n", initial_address, in.first->second);
						return in.first->second;
					}
					size_t initial_pc = program_data.size();
					//log("decoding at 0x%04x: initial_pc %d\n", initial_address, initial_pc);
					in.first->second = initial_pc;
					auto r = base_r;
					r.skip(initial_address);
					bool done = false;
					while (!done) {
						size_t pc = program_data.size();
						size_t cur_address = r.ptr - base_r.ptr;
						if (cur_address != initial_address) {
							auto in = decode_map.emplace(cur_address, pc);
							if (!in.second) {
								//log("0x%04x (0x%x): already decoded, inserting jump\n", cur_address, pc);
								program_data.push_back(opc_goto + 0x808091);
								program_data.push_back(in.first->second);
								break;
							}
						}
						size_t opcode = r.get<uint8_t>();
						if (opcode >= ins_data.size()) xcept("iscript load: at 0x%04x: invalid instruction %d", cur_address, opcode);
						//log("0x%04x (0x%x): opcode %d\n", cur_address, pc, opcode);
						program_data.push_back(opcode + 0x808091);
						const char* c = ins_data[opcode];
						while (*c) {
							if (*c == 's') {
								++c;
								if (*c=='1') program_data.push_back(r.get<int8_t>());
								else if (*c == '2') program_data.push_back(r.get<int16_t>());
							} else if (*c == '1') program_data.push_back(r.get<uint8_t>());
							else if (*c == '2') program_data.push_back(r.get<uint16_t>());
							else if (*c == 'v') {
								int n = r.get<uint8_t>();
								program_data.push_back(n);
								for (; n; --n) program_data.push_back(r.get<uint16_t>());
							} else if (*c == 'j') {
								size_t jump_address = r.get<uint16_t>();
								auto jump_pc_it = decode_map.find(jump_address);
								if (jump_pc_it == decode_map.end()) {
									program_data.pop_back();
									r = base_r;
									r.skip(jump_address);
								} else {
									program_data.push_back(jump_pc_it->second);
									done = true;
								}
							} else if (*c == 'b') {
								size_t branch_address = r.get<uint16_t>();
								branches.emplace_back(branch_address, program_data.size());
								program_data.push_back(0);
							} else if (*c == 'e') {
								done = true;
							}
							++c;
						}
					}
					return initial_pc;
				};
				size_t initial_pc = decode(initial_address);
				while (!branches.empty()) {
					auto v = branches.front();
					branches.pop_front();
					//log("doing branch to 0x%04x (fixup %x)\n", std::get<0>(v), std::get<1>(v));
					size_t pc = decode(std::get<0>(v));
					if ((int)pc != pc) xcept("iscript load: 0x%x does not fit in an int", pc);
					program_data[std::get<1>(v)] = (int)pc;
				}
				return initial_pc;
			};

			auto& anim_funcs = animation_pc[id];

			size_t highest_animation = script_r.get<uint32_t>();
			size_t animations = (highest_animation + 1 + 1)&-2;
			for (size_t i = 0; i < animations; ++i) {
				size_t anim_address = script_r.get<uint16_t>();
				if (!anim_address) {
					anim_funcs.push_back(0);
					continue;
				}
				auto anim_r = base_r;
				anim_r.skip(anim_address);
				anim_funcs.push_back(decode_at(anim_address));
			}
		}

		st.iscript.program_data = std::move(program_data);
		st.iscript.scripts.clear();
		for (auto& v : animation_pc) {
			auto& s = st.iscript.scripts[v.first];
			s.id = v.first;
			s.animation_pc = std::move(v.second);
		}
	};

	auto load_images = [&]() {

		using data_loading::data_reader_le;

		a_vector<uint8_t> data;
		load_data_file(data, "arr\\images.tbl");
		data_reader_le base_r(data.data(), data.data() + data.size());

		auto r = base_r;
		size_t file_count = r.get<uint16_t>();

		a_vector<grp_t> grps;
		a_vector<a_vector<a_vector<xy>>> lo_offsets;

		auto load_grp = [&](data_reader_le r) {
			grp_t grp;
			size_t frame_count = r.get<uint16_t>();
			grp.width = r.get<uint16_t>();
			grp.height = r.get<uint16_t>();
			grp.frames.resize(frame_count);
			for (size_t i = 0; i < frame_count; ++i) {
				auto&f = grp.frames[i];
				f.left = r.get<int8_t>();
				f.top = r.get<int8_t>();
				f.right = r.get<int8_t>();
				f.bottom = r.get<int8_t>();
				size_t file_offset = r.get<uint32_t>();
			}
			size_t index = grps.size();
			grps.push_back(std::move(grp));
			return index;
		};
		auto load_offsets = [&](data_reader_le r) {
			auto base_r = r;
			lo_offsets.emplace_back();
			auto& offs = lo_offsets.back();

			size_t frame_count = r.get<uint32_t>();
			size_t offset_count = r.get<uint32_t>();
			for (size_t f = 0; f < frame_count; ++f) {
				size_t file_offset = r.get<uint32_t>();
				auto r2 = base_r;
				r2.skip(file_offset);
				offs.emplace_back();
				auto& vec = offs.back();
				vec.resize(offset_count);
				for (size_t i = 0; i < offset_count; ++i) {
					int x = r2.get<int8_t>();
					int y = r2.get<int8_t>();
					vec[i] = { x,y };
				}
			}
			
			return lo_offsets.size() - 1;
		};

		a_unordered_map<size_t, size_t> loaded;
		auto load = [&](int index, std::function<size_t(data_reader_le)> f) {
			if (!index) return (size_t)0;
			auto in = loaded.emplace(index, 0);
			if (!in.second) return in.first->second;
			auto r = base_r;
			r.skip(2 + (index - 1) * 2);
			size_t fn_offset = r.get<uint16_t>();
			r = base_r;
			r.skip(fn_offset);
			a_string fn;
			while (char c = r.get<char>()) fn += c;

			a_vector<uint8_t> data;
			load_data_file(data, format("unit\\%s", fn));
			data_reader_le data_r(data.data(), data.data() + data.size());
			size_t loaded_index = f(data_r);
			in.first->second = loaded_index;
			return loaded_index;
		};

		a_vector<size_t> image_grp_index;
		std::array<a_vector<size_t>, 6> lo_indices;

		grps.emplace_back(); // null/invalid entry
		lo_offsets.emplace_back();

		for (int i = 0; i < 999; ++i) {
			const image_type_t*image_type = get_image_type(i);
			image_grp_index.push_back(load(image_type->grp_filename_index, load_grp));
			lo_indices[0].push_back(load(image_type->attack_filename_index, load_offsets));
			lo_indices[1].push_back(load(image_type->damage_filename_index, load_offsets));
			lo_indices[2].push_back(load(image_type->special_filename_index, load_offsets));
			lo_indices[3].push_back(load(image_type->landing_dust_filename_index, load_offsets));
			lo_indices[4].push_back(load(image_type->lift_off_filename_index, load_offsets));
			lo_indices[5].push_back(load(image_type->shield_filename_index, load_offsets));
		}

		st.grps = std::move(grps);
		st.image_grp.resize(image_grp_index.size());
		for (size_t i = 0; i < image_grp_index.size(); ++i) {
			st.image_grp[i] = &st.grps.at(image_grp_index[i]);
		}
		st.lo_offsets = std::move(lo_offsets);
		st.image_lo_offsets.resize(999);
		for (size_t i = 0; i < lo_indices.size(); ++i) {
			for (int i2 = 0; i2 < 999; ++i2) {
				st.image_lo_offsets.at(i2).at(i) = &st.lo_offsets.at(lo_indices[i].at(i2));
			}
		}

	};

	st.flingy_types = data_loading::load_flingy_dat("arr\\flingy.dat");
	st.sprite_types = data_loading::load_sprites_dat("arr\\sprites.dat");
	st.image_types = data_loading::load_images_dat("arr\\images.dat");
	st.order_types = data_loading::load_orders_dat("arr\\orders.dat");

	auto fixup_sprite_type = [&](sprite_type_t*&ptr) {
		size_t index = (size_t)ptr;
		if (index == 517) ptr = nullptr;
		else ptr = get_sprite_type(index);
	};
	auto fixup_image_type = [&](image_type_t*&ptr) {
		size_t index = (size_t)ptr;
		if (index == 999) ptr = nullptr;
		else ptr = get_image_type(index);
	};

	for (auto&v : st.flingy_types.vec) {
		fixup_sprite_type(v.sprite);
	}
	for (auto&v : st.sprite_types.vec) {
		fixup_image_type(v.image);
	}

	load_iscript_bin();
	load_images();

	// This function returns (int)std::round(std::sin(PI / 128 * i) * 256) for i [0, 63]
	// using only integer arithmetic.
	auto int_sin = [&](int x) {
		int x2 = x*x;
		int x3 = x2*x;
		int x4 = x3*x;
		int x5 = x4*x;

		int64_t a0 = 26980449732;
		int64_t a1 = 1140609;
		int64_t a2 = -2785716;
		int64_t a3 = 2159;
		int64_t a4 = 58;

		return (int)((x * a0 + x2 * a1 + x3 * a2 + x4 * a3 + x5 * a4 + ((int64_t)1 << 31)) >> 32);
	};

	// The sin lookup table is hardcoded into Broodwar. We generate it here.
	for (int i = 0; i <= 64; ++i) {
		auto v = fp8::from_raw(int_sin(i));
		st.direction_table[i].x = v;
		st.direction_table[64 - i].y = -v;
		st.direction_table[64 + (64 - i)].x = v;
		st.direction_table[64 + i].y = v;
		st.direction_table[128 + i].x = -v;
		st.direction_table[128 + (64 - i)].y = v;
		st.direction_table[(192 + (64 - i)) % 256].x = -v;
		st.direction_table[(192 + i) % 256].y = -v;
	}

}

void init() {

	global_state global_st;
	game_state game_st;
	state st;
	st.global = &global_st;
	st.game = &game_st;

	global_init(global_st);

	game_load_functions game_load_funcs(st);
	game_load_funcs.load_map_file(R"(X:\Starcraft\StarCraft\maps\testone.scm)");

	for (size_t i = 0; i != 8034; ++i) {
		advance(st);

		log("%d: advance yey\n", i);
	}

	advance(st);

}

}

