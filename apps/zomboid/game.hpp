#pragma once

#include <string>

#include "maz/scene/SceneTree.hpp"

// ZOMBOID — a native top-down zombie-survival game built entirely on the Maz Engine: its whole
// simulation (survivor needs, zombie chase-and-attack AI, loot) is written in maz::script and driven
// through a scene::SceneTree, exactly the way you'd author a game in Godot. This header holds the
// game's script program and a scene builder so both the playable app and the unit tests share one
// source of truth. The rules run deterministically and headless — the app layer only renders them.
namespace zomboid {

// The complete game logic, in maz::script. A shared global `g_player` (set by the survivor in
// _ready) lets every zombie reference the survivor and call its methods — cross-object gameplay
// with zero host coupling. Everything else is per-instance state and lifecycle hooks the SceneTree
// drives each frame.
inline const char* scripts() {
    return R"MAZ(
# The survivor the player controls. Hunger rises over time; at max hunger, health drains.
var g_player = nil;

class Survivor {
    var health = 100;
    var hunger = 0;
    var alive = true;
    var food = 3;      # ration count
    var kills = 0;

    func _ready() { g_player = self; }

    func _process(dt) {
        if (self.alive == false) { return; }
        self.hunger = self.hunger + dt * 3;
        if (self.hunger > 100) { self.hunger = 100; }
        if (self.hunger >= 100) { self.health = self.health - dt * 4; }
        if (self.health <= 0) { self.health = 0; self.alive = false; }
    }

    # Eat a ration: costs one food, restores hunger.
    func eat() {
        if (self.food > 0) {
            self.food = self.food - 1;
            self.hunger = self.hunger - 40;
            if (self.hunger < 0) { self.hunger = 0; }
        }
    }

    func take_damage(dmg) {
        self.health = self.health - dmg;
        if (self.health <= 0) { self.health = 0; self.alive = false; }
    }
}

# A zombie: walks straight at the survivor and bites when in range (on a cooldown).
class Zombie {
    var speed = 15;
    var damage = 6;
    var attack_range = 1.0;
    var cooldown = 0;

    func _process(dt) {
        if (g_player == nil) { return; }
        if (g_player.alive == false) { return; }
        var dx = g_player.node.x - self.node.x;
        var dy = g_player.node.y - self.node.y;
        var dist = sqrt(dx * dx + dy * dy);
        if (dist > self.attack_range) {
            self.node.x = self.node.x + (dx / dist) * self.speed * dt;
            self.node.y = self.node.y + (dy / dist) * self.speed * dt;
        }
        self.cooldown = self.cooldown - dt;
        if (dist <= self.attack_range and self.cooldown <= 0) {
            g_player.take_damage(self.damage);
            self.cooldown = 1.0;
        }
    }
}

# A pickup the survivor can collect for food.
class Loot {
    var kind = "ration";
    var taken = false;
}
)MAZ";
}

// Build a starting scene: one survivor at the origin, a ring of zombies, and some loot. Returns a
// pointer to the survivor node so the app/test can read its stats and move it with input.
inline maz::scene::SceneNode* buildScene(maz::scene::SceneTree& tree) {
    tree.loadScripts(scripts());

    maz::scene::SceneNode* survivor = tree.createChild(tree.root(), "Survivor");
    survivor->setPosition(0, 0);
    survivor->addToGroup("player");
    tree.attachScript(*survivor, "Survivor");

    // A ring of zombies closing in from the edges.
    const int kZombies = 6;
    const double kRadius = 30.0;
    for (int i = 0; i < kZombies; ++i) {
        const double ang = 6.28318530718 * i / kZombies;
        maz::scene::SceneNode* z = tree.createChild(tree.root(), "Zombie" + std::to_string(i));
        z->setPosition(kRadius * std::cos(ang), kRadius * std::sin(ang));
        z->addToGroup("zombies");
        tree.attachScript(*z, "Zombie");
    }

    // Scattered loot.
    for (int i = 0; i < 3; ++i) {
        maz::scene::SceneNode* l = tree.createChild(tree.root(), "Loot" + std::to_string(i));
        l->setPosition(-10.0 + i * 10.0, 12.0);
        l->addToGroup("loot");
        tree.attachScript(*l, "Loot");
    }

    return survivor;
}

} // namespace zomboid
