#pragma once
#include "State.h"
struct SpellEffect;
struct BuffDef;
struct Character;

struct SpellObjectDef
{
  std::string name;
  float32 speed;
  std::vector<SpellEffect *> effects;
};

struct SpellObjectInst
{
  SpellObjectDef def;
  Character *caster;
  Character *target;
  vec3 pos;
};

enum SpellTargets
{
  Self,
  Ally,
  Hostile,
  Terrain
};

struct SpellDef
{
  std::string name;
  int mana_cost;
  float32 range;
  SpellTargets targets;
  float32 cooldown;
  float32 cast_time;
  std::vector<SpellEffect *> effects;
};

struct Spell
{
  SpellDef *def;
  float32 cd_remaining;
};

struct DamageEffect
{
  int n;
  bool pierce_absorb;
  bool pierce_mod;
};

struct HealEffect
{
  int n;
};

struct ApplyBuffEffect
{
  BuffDef *buff;
};

struct ApplyDebuffEffect
{
  BuffDef *debuff;
};

struct AoEEffect
{
  SpellTargets targets;
  float32 radius;
  SpellEffect *effect;
};

struct ClearDebuffsEffect
{
};

struct ObjectLaunchEffect
{
  SpellObjectDef *object;
};

struct InterruptEffect
{
  float32 lockout;
};

enum SpellEffectType
{
  Heal,
  Damage,
  ApplyBuff,
  ApplyDebuff,
  AoE,
  ClearDebuffs,
  ObjectLaunch,
  Interrupt
};

struct SpellEffect
{
  std::string name;
  SpellEffectType type;
  union {
    HealEffect heal;
    DamageEffect damage;
    ApplyBuffEffect applybuff;
    ApplyDebuffEffect applydebuff;
    AoEEffect aoe;
    ClearDebuffsEffect cleardebuffs;
    ObjectLaunchEffect objectlaunch;
    InterruptEffect interrupt;
  };
};
struct SpellEffectInst
{
  SpellEffect def;
  Character *caster;
  vec3 pos;
  Character *target;
};

struct DamageTakenMod
{
  float32 n;
};

struct SpeedMod
{
  float32 m;
};

struct CastSpeedMod
{
  float32 m;
};

struct SilenceMod
{
};

enum CharModType
{
  DamageTaken,
  Speed,
  CastSpeed,
  Silence
};

struct CharMod
{
  CharModType type;
  union {
    DamageTakenMod damage_taken;
    SpeedMod speed;
    CastSpeedMod cast_speed;
    SilenceMod silence;
  };
};

struct BuffDef
{
  std::string name;
  float32 duration;
  float32 tick_freq;
  std::vector<SpellEffect *> tick_effects;
  std::vector<CharMod *> char_mods;
};

struct Buff
{
  BuffDef def;
  float64 duration;
  uint32 ticks_left = 0;
  bool dynamic = false;
};

struct CharStats
{
  float32 gcd;
  float32 speed;
  float32 cast_speed;
  float32 cast_damage;
  float32 mana_regen;
  float32 hp_regen;
  float32 damage_mod;
  float32 atk_dmg;
  float32 atk_speed;
};

struct Character
{
  Cylinder body;
  Node_Ptr mesh;

  vec3 pos;
  vec3 dir;

  std::string name;
  int team;
  Character *target = nullptr;

  CharStats b_stats, e_stats;

  std::map<std::string, Spell> spellbook;

  std::vector<Buff> buffs, debuffs;

  int32 hp, hp_max;
  float32 mana, mana_max;
  bool alive = true;

  float64 atk_cd = 0;
  float64 gcd = 0;

  bool silenced = false;
  bool casting = false;
  Spell *casting_spell;
  float32 cast_progress = 0;
  Character *cast_target = nullptr;
};

struct Warg_State : protected State
{
  Warg_State(std::string name, SDL_Window *window, ivec2 window_size);
  void update();
  void handle_input(State **current_state,
    std::vector<State *> available_states);
  vec3 player_pos = vec3(0, 0, 0.5);
  vec3 player_dir = vec3(0, 1, 0);

  void add_char(int team, std::string name);

  std::array<Character, 10> chars;
  int pc = 0;

  std::vector<SpellObjectInst> spell_objs;

  vec3 spawnloc0 = vec3(8, 2, 0.5f);
  vec3 spawnloc1 = vec3(8, 20, 0.5f);
  vec3 spawndir0 = vec3(0, 1, 0);
  vec3 spawndir1 = vec3(0, -1, 0);

  vec3 ground_pos = vec3(8, 11, 0);// -0.1f); ??
  vec3 ground_dim = vec3(16, 22, 0.05);
  vec3 ground_dir = vec3(0, 1, 0);
  Node_Ptr ground_mesh;
  void add_wall(vec3 p1, vec2 p2, float32 h);
  std::vector<Wall> walls;
  std::vector<Node_Ptr> wall_meshes;


  std::array<CharMod, 100> char_mods;
  std::array<BuffDef, 100> buffs;
  std::array<SpellDef, 100> spells;
  std::array<SpellEffect, 100> spell_effects;
  std::array<SpellObjectDef, 100> spell_objects;
  uint32 nchar_mods = 0;
  uint32 nbuffs = 0;
  uint32 nspell_effects = 0;
  uint32 nspell_objects = 0;
  uint32 nspells = 0;
  uint32 nchars = 0;
};