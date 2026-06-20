#!/usr/bin/env python3
"""Tiny SQLite live DB for the QQXJ private-server stub."""

from __future__ import annotations

import argparse
import json
import sqlite3
from pathlib import Path


DEFAULT_DB = Path(__file__).resolve().parent / "live" / "blue_tears_live.sqlite3"


DEMO_CHARACTERS = [
    {
        "permanent_id": 100001,
        "account_id": 1,
        "name": "DemoHero",
        "job_class": "JobClassWarrior",
        "sub_class": "Berserker",
        "level": 60,
        "exp": 0,
        "class_level": 10,
        "money": 99999,
        "ability_point": 50,
        "skill_point": 300,
        "skill_book_level": 60,
        "luck": 0,
        "hyper_gauge": 400,
        "hyper_gauge_max": 400,
        "hyper_gauge_regen": 20,
        "stamina": 100,
        "map_name": "Intro_whiteCastle",
        "room_index": 0,
        "pos_x": -228.431,
        "pos_y": 11.4057,
        "pos_z": 39.2735,
        "dir_x": 0.0,
        "dir_y": 0.0,
        "dir_z": 1.0,
        "gender": "Male",
        "hair_style": "THairMWindCut",
        "eyebrow": "TTemplateEyebrowm10",
        "eye": "TTemplateEyem10",
        "mouth": "TTemplateMouthm10",
        "nose": "Default_Nose",
        "hair_color_argb": 0xFFCCB070,
        "skin_color": "SkinColor4",
        "body_size": 0.5,
        "head_size": 0.7,
        "scale_by_age": 0.5,
        "armor_or_casual": "Casual",
        "stats": {
            "Level": 60,
            "Exp": 0,
            "ClassLevel": 10,
            "Money": 99999,
            "AbilityPoint": 50,
            "SkillPoint": 300,
            "SkillBookLevel": 60,
            "HP": 6000,
            "MP": 4000,
            "HPMax": 6000,
            "MPMax": 4000,
            "MoveSpeed": 31.5,
            "AttackSpeed": 25,
            "HyperGauge": 400,
            "HyperGaugeMax": 400,
            "HyperGaugeRegen": 20,
            "Stamina": 100,
            "Luck": 0,
            "MinPhysicalAttackPower": 500,
            "MaxPhysicalAttackPower": 1000,
            "PhysicalDefensePower": 200,
            "MagicalDefensePower": 100,
        },
        "skills": {
            "TSkill_Warrior_Strike": 10,
            "TSkill_Warrior_Slash": 10,
            "TSkill_Warrior_Mastery2HDSWD": 5,
            "TSkill_Warrior_SoulSlash": 5,
        },
    },
    {
        "permanent_id": 100002,
        "account_id": 1,
        "name": "DemoWizard",
        "job_class": "JobClassMagician",
        "sub_class": "FireMagician",
        "level": 60,
        "exp": 0,
        "class_level": 10,
        "money": 99999,
        "ability_point": 50,
        "skill_point": 300,
        "skill_book_level": 60,
        "luck": 0,
        "hyper_gauge": 300,
        "hyper_gauge_max": 300,
        "hyper_gauge_regen": 21,
        "stamina": 100,
        "map_name": "Intro_whiteCastle",
        "room_index": 0,
        "pos_x": -228.431,
        "pos_y": 11.4057,
        "pos_z": 39.2735,
        "dir_x": 0.0,
        "dir_y": 0.0,
        "dir_z": 1.0,
        "gender": "Female",
        "hair_style": "THairWLovelyCurl",
        "eyebrow": "TTemplateEyebroww10",
        "eye": "TTemplateEyew10",
        "mouth": "TTemplateMouthw10",
        "nose": "Default_Nose",
        "hair_color_argb": 0xFFFF4060,
        "skin_color": "SkinColor2",
        "body_size": 0.5,
        "head_size": 0.7,
        "scale_by_age": 0.5,
        "armor_or_casual": "Casual",
        "stats": {
            "Level": 60,
            "Exp": 0,
            "ClassLevel": 10,
            "Money": 99999,
            "AbilityPoint": 50,
            "SkillPoint": 300,
            "SkillBookLevel": 60,
            "HP": 6000,
            "MP": 6000,
            "HPMax": 6000,
            "MPMax": 6000,
            "MoveSpeed": 31.5,
            "AttackSpeed": 20,
            "HyperGauge": 300,
            "HyperGaugeMax": 300,
            "HyperGaugeRegen": 21,
            "Stamina": 100,
            "Luck": 0,
            "MinMagicalAttackPower": 500,
            "MaxMagicalAttackPower": 1000,
            "FirePower": 500,
        },
        "skills": {
            "TSkill_Magician_Teleport": 5,
            "TSkill_Magician_HyperTeleport": 1,
            "TSkill_Magician_FireBall": 10,
            "TSkill_Magician_FireOrbCircle": 5,
            "TSkill_Magician_ChargingEnergyBall": 5,
            "TSkill_Magician_EnergyBolt": 10,
            "TSkill_Magician_EnergyShield": 5,
            "TSkill_Magician_Meditation": 5,
            "TSkill_Magician_Hypermnesia": 5,
            "TSkill_Magician_ManaExpansion": 5,
        },
    },
    {
        "permanent_id": 100003,
        "account_id": 1,
        "name": "DemoFrostia",
        "job_class": "JobClassMagician",
        "sub_class": "IceMagician",
        "level": 60,
        "exp": 0,
        "class_level": 10,
        "money": 99999,
        "ability_point": 50,
        "skill_point": 300,
        "skill_book_level": 60,
        "luck": 0,
        "hyper_gauge": 300,
        "hyper_gauge_max": 300,
        "hyper_gauge_regen": 21,
        "stamina": 100,
        "map_name": "Intro_whiteCastle",
        "room_index": 0,
        "pos_x": -228.431,
        "pos_y": 11.4057,
        "pos_z": 39.2735,
        "dir_x": 0.0,
        "dir_y": 0.0,
        "dir_z": 1.0,
        "gender": "Female",
        "hair_style": "THairWAntennaShort",
        "eyebrow": "TTemplateEyebroww11",
        "eye": "TTemplateEyew11",
        "mouth": "TTemplateMouthw11",
        "nose": "Default_Nose",
        "hair_color_argb": 0xFF4080FF,
        "skin_color": "SkinColor1",
        "body_size": 0.5,
        "head_size": 0.7,
        "scale_by_age": 0.5,
        "armor_or_casual": "Casual",
        "stats": {
            "Level": 60,
            "Exp": 0,
            "ClassLevel": 10,
            "Money": 99999,
            "AbilityPoint": 50,
            "SkillPoint": 300,
            "SkillBookLevel": 60,
            "HP": 6000,
            "MP": 6000,
            "HPMax": 6000,
            "MPMax": 6000,
            "HPRegen": 4,
            "MPRegen": 10,
            "MoveSpeed": 31.5,
            "AttackSpeed": 20,
            "HyperGauge": 300,
            "HyperGaugeMax": 300,
            "HyperGaugeRegen": 21,
            "Stamina": 100,
            "Luck": 0,
            "MinPhysicalAttackPower": 50,
            "MaxPhysicalAttackPower": 100,
            "MinMagicalAttackPower": 500,
            "MaxMagicalAttackPower": 1000,
            "MagicalOffensivePower": 800,
            "MagicalOffensiveScope": 50,
            "IcePower": 500,
            "FirePower": 100,
            "LightningPower": 100,
            "DarkPower": 100,
            "CriticalPowerAdd": 50,
            "PhysicalDefensePower": 100,
            "MagicalDefensePower": 200,
            "PhysicalResistance": 20,
            "MagicalResistance": 40,
            "FireResistance": 20,
            "IceResistance": 50,
            "LightningResistance": 20,
            "DarkResistance": 20,
        },
        "skills": {
            "TSkill_Magician_Teleport": 5,
            "TSkill_Magician_HyperTeleport": 1,
            "TSkill_Magician_ChargingEnergyBall": 5,
            "TSkill_Magician_EnergyBolt": 10,
            "TSkill_Magician_EnergyBolt2": 10,
            "TSkill_Magician_EnergyShield": 5,
            "TSkill_Magician_EnergyShieldParty": 5,
            "TSkill_Magician_Meditation": 5,
            "TSkill_Magician_Hypermnesia": 5,
            "TSkill_Magician_ManaExpansion": 5,
            "TSkill_Magician_IceBall": 10,
            "TSkill_Magician_IceShield": 10,
        },
    },
    {
        "permanent_id": 100004,
        "account_id": 1,
        "name": "DemoHunter",
        "job_class": "JobClassRanger",
        "sub_class": "PowerfulRanger",
        "level": 60,
        "exp": 0,
        "class_level": 10,
        "money": 99999,
        "ability_point": 50,
        "skill_point": 300,
        "skill_book_level": 60,
        "luck": 0,
        "hyper_gauge": 260,
        "hyper_gauge_max": 260,
        "hyper_gauge_regen": 20,
        "stamina": 100,
        "map_name": "Intro_whiteCastle",
        "room_index": 0,
        "pos_x": -228.431,
        "pos_y": 11.4057,
        "pos_z": 39.2735,
        "dir_x": 0.0,
        "dir_y": 0.0,
        "dir_z": 1.0,
        "gender": "Male",
        "hair_style": "THairMWindCut",
        "eyebrow": "TTemplateEyebrowm10",
        "eye": "TTemplateEyem10",
        "mouth": "TTemplateMouthm10",
        "nose": "Default_Nose",
        "hair_color_argb": 0xFFCCB070,
        "skin_color": "SkinColor4",
        "body_size": 0.5,
        "head_size": 0.7,
        "scale_by_age": 0.5,
        "armor_or_casual": "Casual",
        "stats": {
            "Level": 60,
            "Exp": 0,
            "ClassLevel": 10,
            "Money": 99999,
            "AbilityPoint": 50,
            "SkillPoint": 300,
            "SkillBookLevel": 60,
            "HP": 6000,
            "MP": 4000,
            "HPMax": 6000,
            "MPMax": 4000,
            "MoveSpeed": 31.5,
            "AttackSpeed": 25,
            "HyperGauge": 260,
            "HyperGaugeMax": 260,
            "HyperGaugeRegen": 20,
            "Stamina": 100,
            "Luck": 0,
            "MinPhysicalAttackPower": 500,
            "MaxPhysicalAttackPower": 1000,
            "PhysicalOffensivePower": 800,
        },
        "skills": {
            "TSkill_Ranger_DoubleJump": 5,
            "TSkill_Ranger_ChainShot": 10,
            "TSkill_Ranger_HeavyShot": 10,
        },
    },
]


SCHEMA = """
CREATE TABLE IF NOT EXISTS accounts (
    id INTEGER PRIMARY KEY,
    name TEXT UNIQUE NOT NULL,
    password TEXT NOT NULL DEFAULT '',
    gender INTEGER NOT NULL DEFAULT 0,
    cash INTEGER NOT NULL DEFAULT 0,
    created TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_login TEXT
);

CREATE TABLE IF NOT EXISTS characters (
    permanent_id INTEGER PRIMARY KEY,
    account_id INTEGER NOT NULL,
    name TEXT NOT NULL,
    job_class TEXT NOT NULL,
    sub_class TEXT NOT NULL,
    db_version INTEGER NOT NULL DEFAULT 64,
    level INTEGER NOT NULL DEFAULT 1,
    exp INTEGER NOT NULL DEFAULT 0,
    class_level INTEGER NOT NULL DEFAULT 0,
    money INTEGER NOT NULL DEFAULT 0,
    ability_point INTEGER NOT NULL DEFAULT 0,
    skill_point INTEGER NOT NULL DEFAULT 0,
    skill_point_used INTEGER NOT NULL DEFAULT 0,
    skill_book_level INTEGER NOT NULL DEFAULT 0,
    luck INTEGER NOT NULL DEFAULT 0,
    hyper_gauge INTEGER,
    hyper_gauge_max INTEGER,
    hyper_gauge_regen INTEGER,
    stamina INTEGER,
    marble_jam INTEGER NOT NULL DEFAULT 0,
    map_name TEXT NOT NULL DEFAULT 'Intro_whiteCastle',
    room_index INTEGER NOT NULL DEFAULT 0,
    pos_x REAL NOT NULL DEFAULT -228.431,
    pos_y REAL NOT NULL DEFAULT 11.4057,
    pos_z REAL NOT NULL DEFAULT 39.2735,
    dir_x REAL NOT NULL DEFAULT 0,
    dir_y REAL NOT NULL DEFAULT 0,
    dir_z REAL NOT NULL DEFAULT 1,
    room_back TEXT NOT NULL DEFAULT '',
    room_back_pos TEXT NOT NULL DEFAULT '',
    gender TEXT NOT NULL DEFAULT 'Male',
    hair_style TEXT NOT NULL DEFAULT '',
    eyebrow TEXT NOT NULL DEFAULT '',
    eye TEXT NOT NULL DEFAULT '',
    mouth TEXT NOT NULL DEFAULT '',
    nose TEXT NOT NULL DEFAULT 'Default_Nose',
    hair_color_argb INTEGER NOT NULL DEFAULT 0,
    skin_color TEXT NOT NULL DEFAULT '',
    body_size REAL NOT NULL DEFAULT 0.5,
    head_size REAL NOT NULL DEFAULT 0.7,
    scale_by_age REAL NOT NULL DEFAULT 0.5,
    scale_head REAL NOT NULL DEFAULT 0,
    scale_arm_width REAL NOT NULL DEFAULT 0,
    scale_arm_height REAL NOT NULL DEFAULT 0,
    scale_leg_width REAL NOT NULL DEFAULT 0,
    scale_leg_height REAL NOT NULL DEFAULT 0,
    armor_or_casual TEXT NOT NULL DEFAULT 'Casual',
    current_vehicle TEXT NOT NULL DEFAULT '',
    current_trans TEXT NOT NULL DEFAULT '',
    cas_opt_num INTEGER NOT NULL DEFAULT 63,
    hp INTEGER,
    mp INTEGER,
    hp_max INTEGER,
    mp_max INTEGER,
    bind_instance_map_key TEXT,
    is_in_start INTEGER NOT NULL DEFAULT 1,
    quit_theme TEXT,
    stats_json TEXT NOT NULL DEFAULT '{}',
    equipped_json TEXT NOT NULL DEFAULT '{}',
    last_save_time TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS items (
    oid INTEGER PRIMARY KEY,
    uid INTEGER UNIQUE,
    template_name TEXT NOT NULL,
    owner_char_id INTEGER,
    container_tab TEXT,
    container_slot INTEGER,
    equipped_section TEXT,
    equipped_parts TEXT,
    durability INTEGER,
    identified INTEGER NOT NULL DEFAULT 1,
    stack_count INTEGER NOT NULL DEFAULT 1,
    enchant_cnt INTEGER NOT NULL DEFAULT 0,
    binded INTEGER NOT NULL DEFAULT 0,
    socket_keys TEXT,
    socket_data BLOB,
    bt1 INTEGER, bt2 INTEGER, bt3 INTEGER,
    it1 INTEGER, it2 INTEGER,
    creation_info BLOB,
    modifications BLOB,
    strengthen_hist BLOB,
    item_features BLOB,
    expire_time TEXT
);

CREATE TABLE IF NOT EXISTS character_skills (
    character_id INTEGER NOT NULL,
    skill_name TEXT NOT NULL,
    skill_level INTEGER NOT NULL DEFAULT 1,
    skill_points INTEGER NOT NULL DEFAULT 0,
    hotkey_slot INTEGER NOT NULL DEFAULT -1,
    PRIMARY KEY (character_id, skill_name)
);

CREATE TABLE IF NOT EXISTS character_quests (
    character_id INTEGER NOT NULL,
    quest_id TEXT NOT NULL,
    state TEXT NOT NULL DEFAULT 'Active',
    progress BLOB,
    started TEXT,
    completed TEXT,
    PRIMARY KEY (character_id, quest_id)
);

CREATE TABLE IF NOT EXISTS character_buffs (
    character_id INTEGER NOT NULL,
    buff_name TEXT NOT NULL,
    duration_ms INTEGER NOT NULL DEFAULT 0,
    flags INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (character_id, buff_name)
);
"""


CHAR_COLUMNS = [
    "permanent_id", "account_id", "name", "job_class", "sub_class",
    "db_version", "level", "exp", "class_level", "money",
    "ability_point", "skill_point", "skill_point_used", "skill_book_level",
    "luck", "hyper_gauge", "hyper_gauge_max", "hyper_gauge_regen", "stamina",
    "marble_jam", "map_name", "room_index", "pos_x", "pos_y", "pos_z",
    "dir_x", "dir_y", "dir_z", "room_back", "room_back_pos", "gender",
    "hair_style", "eyebrow", "eye", "mouth", "nose", "hair_color_argb",
    "skin_color", "body_size", "head_size", "scale_by_age", "scale_head",
    "scale_arm_width", "scale_arm_height", "scale_leg_width",
    "scale_leg_height", "armor_or_casual", "current_vehicle",
    "current_trans", "cas_opt_num", "hp", "mp", "hp_max", "mp_max",
    "bind_instance_map_key", "is_in_start", "quit_theme", "stats_json",
    "equipped_json",
]


def _connect(path: Path) -> sqlite3.Connection:
    path.parent.mkdir(parents=True, exist_ok=True)
    con = sqlite3.connect(path)
    con.row_factory = sqlite3.Row
    return con


def _char_row(template: dict) -> dict:
    stats = dict(template.get("stats", {}))
    row = {name: template.get(name) for name in CHAR_COLUMNS}
    row.update(
        db_version=template.get("db_version", 64),
        skill_point_used=template.get("skill_point_used", 0),
        marble_jam=template.get("marble_jam", 0),
        room_back=template.get("room_back", ""),
        room_back_pos=template.get("room_back_pos", ""),
        scale_head=template.get("scale_head", 0),
        scale_arm_width=template.get("scale_arm_width", 0),
        scale_arm_height=template.get("scale_arm_height", 0),
        scale_leg_width=template.get("scale_leg_width", 0),
        scale_leg_height=template.get("scale_leg_height", 0),
        current_vehicle=template.get("current_vehicle", ""),
        current_trans=template.get("current_trans", ""),
        cas_opt_num=template.get("cas_opt_num", 63),
        hp=stats.get("HP"),
        mp=stats.get("MP"),
        hp_max=stats.get("HPMax"),
        mp_max=stats.get("MPMax"),
        bind_instance_map_key=template.get("bind_instance_map_key"),
        is_in_start=1,
        quit_theme=template.get("quit_theme"),
        stats_json=json.dumps(stats, sort_keys=True),
        equipped_json=json.dumps(template.get("equipped", {}), sort_keys=True),
    )
    return row


def init_db(path: Path = DEFAULT_DB, reset: bool = False) -> Path:
    if reset and path.exists():
        path.unlink()
    with _connect(path) as con:
        con.executescript(SCHEMA)
        con.execute(
            """
            INSERT INTO accounts (id, name, password, gender, cash)
            VALUES (1, 'demo', '', 0, 0)
            ON CONFLICT(id) DO UPDATE SET name=excluded.name
            """
        )
        placeholders = ", ".join("?" for _ in CHAR_COLUMNS)
        updates = ", ".join(f"{name}=excluded.{name}" for name in CHAR_COLUMNS if name != "permanent_id")
        sql = (
            f"INSERT INTO characters ({', '.join(CHAR_COLUMNS)}) "
            f"VALUES ({placeholders}) ON CONFLICT(permanent_id) DO UPDATE SET {updates}"
        )
        for char in DEMO_CHARACTERS:
            row = _char_row(char)
            con.execute(sql, [row[name] for name in CHAR_COLUMNS])
            con.execute("DELETE FROM character_skills WHERE character_id = ?", (char["permanent_id"],))
            for skill, level in char.get("skills", {}).items():
                con.execute(
                    """
                    INSERT INTO character_skills
                    (character_id, skill_name, skill_level, skill_points, hotkey_slot)
                    VALUES (?, ?, ?, 0, -1)
                    """,
                    (char["permanent_id"], skill, level),
                )
        con.commit()
    return path


def load_account(path: Path = DEFAULT_DB, account_name: str = "demo") -> dict:
    with _connect(path) as con:
        account = con.execute("SELECT * FROM accounts WHERE name = ?", (account_name,)).fetchone()
        if account is None:
            raise KeyError(f"account not found: {account_name}")
        chars = con.execute(
            "SELECT * FROM characters WHERE account_id = ? ORDER BY permanent_id",
            (account["id"],),
        ).fetchall()
        out_chars = []
        for row in chars:
            char = dict(row)
            char["stats"] = json.loads(char.pop("stats_json") or "{}")
            char["equipped"] = json.loads(char.pop("equipped_json") or "{}")
            skills = con.execute(
                "SELECT skill_name, skill_level FROM character_skills WHERE character_id = ?",
                (char["permanent_id"],),
            ).fetchall()
            char["skills"] = {skill["skill_name"]: skill["skill_level"] for skill in skills}
            out_chars.append(char)
        return {"account": dict(account), "characters": out_chars}


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", default=str(DEFAULT_DB))
    ap.add_argument("--reset", action="store_true")
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()
    path = init_db(Path(args.db), reset=args.reset)
    if args.list:
        bundle = load_account(path)
        print(f"db={path}")
        print(f"account={bundle['account']['name']} characters={len(bundle['characters'])}")
        for char in bundle["characters"]:
            print(f"- {char['permanent_id']} {char['name']} {char['job_class']}/{char['sub_class']} lv{char['level']}")
    else:
        print(path)


if __name__ == "__main__":
    main()
