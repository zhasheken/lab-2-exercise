#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <random>
#include <vector>

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window.hpp>

// Define M_PI if not already defined
#ifndef M_PI
#define M_PI 3.1415926
#endif

const unsigned int WINDOW_WIDTH = 800;
const unsigned int WINDOW_HEIGHT = 800;
const sf::Vector2f WINDOW_MIDDLE = {static_cast<float>(WINDOW_WIDTH / 2),
                                    static_cast<float>(WINDOW_HEIGHT / 2)};

const float SPACESHIP_LENGTH = 35.f;         // length of largest dimension of spaceship in pixels
const float SPACESHIP_HITBOX_RADIUS = 25.f;  // => diameter of 50, slightly larger than spaceship
const float SPACESHIP_SPEED = 5.f;

const float ASTEROID_RADIUS = 40.f;

const float BULLET_RADIUS = 3.f;
const float BULLET_SPEED = 8.f;
const float SHOOT_COOLDOWN = 0.1f;  // 100ms between shots

float distance(sf::Vector2f p1, sf::Vector2f p2) { return (p1 - p2).length(); }

// (p1, r1) = (position of center of circle 1, radius of circle 1)
// (p2, r2) = (position of center of circle 2, radius of circle 2)
bool circlesIntersect(sf::Vector2f p1, float r1, sf::Vector2f p2, float r2) {
    return distance(p1, p2) <= (r1 + r2);
}

// "Statistics" that should be captured from the input handling stage.
struct InputSummary {
    sf::Vector2f movementDirection;  // unit vector for direction of movement
    sf::Vector2f targetPosition;     // i.e., where the mouse is.
    bool shootingDesired;  // whether or not the user indicates to shoot (regardless of cooldown)
};

// Input handling
InputSummary processInputs(sf::Window& window, bool& shouldClose) {
    InputSummary inputSummary{};  // value initialization defaults shootingDesired = false.
    for (auto event = window.pollEvent(); event.has_value(); event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window.close();
            shouldClose = true;
            return {};
        }
    }
    // --- Movement ---
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) {
        inputSummary.movementDirection += {0.f, -1.f};
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) {
        inputSummary.movementDirection += {0.f, 1.f};
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) {
        inputSummary.movementDirection += {-1.f, 0.f};
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
        inputSummary.movementDirection += {1.f, 0.f};
    }
    // --- movement direction ---
    inputSummary.movementDirection = (inputSummary.movementDirection.length() > 0)
                                         ? inputSummary.movementDirection.normalized()
                                         : inputSummary.movementDirection;

    // --- target position ---
    inputSummary.targetPosition = sf::Vector2<float>(sf::Mouse::getPosition(window));
    // --- shooting desired ---
    inputSummary.shootingDesired = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) ||
                                   sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
    return inputSummary;
}

// As in the prep, this has been simplified to hardcoded values to make your life easier
// for a short lab.
class ResourceManager {
public:
    sf::Texture spaceshipTexture;
    sf::SoundBuffer explosionSoundBuffer;
};

// Simple struct for asteroids.
struct Asteroid : public sf::Drawable {
    Asteroid(sf::Vector2f startPosition, sf::Vector2f asteroidVelocity, float initialRadius)
        : velocity(asteroidVelocity), isAlive(true) {
        shape.setRadius(initialRadius);
        shape.setFillColor(sf::Color(100, 100, 100));  // Gray
        shape.setOutlineThickness(2.0f);
        shape.setOutlineColor(sf::Color(50, 50, 50));                 // Dark outline
        shape.setOrigin(sf::Vector2f(initialRadius, initialRadius));  // Center origin
        shape.setPosition(startPosition);
    }

    void update() {
        shape.move(velocity);
        // Simple wrapping for asteroids
        const float radius = shape.getRadius();
        sf::Vector2f astPos = shape.getPosition();
        if (astPos.x < -radius) astPos.x = static_cast<float>(WINDOW_WIDTH) + radius;
        if (astPos.x > static_cast<float>(WINDOW_WIDTH) + radius) astPos.x = -radius;
        if (astPos.y < -radius) astPos.y = static_cast<float>(WINDOW_HEIGHT) + radius;
        if (astPos.y > static_cast<float>(WINDOW_HEIGHT) + radius) astPos.y = -radius;
        shape.setPosition(astPos);
    }

    void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
        target.draw(shape, states);
    }

    sf::CircleShape shape;
    sf::Vector2f velocity;
    bool isAlive;
};

// Define a struct for bullets
struct Bullet : public sf::Drawable {
    Bullet(sf::Vector2f startPosition, sf::Vector2f bulletVelocity)
        : velocity(bulletVelocity),
          lifetime(3.0f)  // Bullets live for 3 seconds
          ,
          isAlive(true) {
        shape.setRadius(BULLET_RADIUS);
        shape.setFillColor(sf::Color::Red);
        shape.setOrigin(sf::Vector2f(BULLET_RADIUS, BULLET_RADIUS));  // Center origin
        shape.setPosition(startPosition);
    }

    void update() {
        // =====
        // TODO: Implement bullet update mechanics. In detail:
        //  - Move bullet's shape using bullet's velocity
        //  - Decrease bullet lifetime by 1.0f / 60.0f (60 FPS)
        //  - Mark bullets as dead (bullet.isAlive = false) if:
        //      - lifetime <= 0.0f, or
        //      - bullet is off screen (use shape.getPosition() and
        //        WINDOW_WIDTH and WINDOW_HEIGHT)
        shape.move(velocity);
        lifetime -= 1.0f / 60.0f;
        if (lifetime <= 0.0f) {
            isAlive = false;
        }
        sf::Vector2f bulletPos = shape.getPosition();
        // bullets areoff screen if their center is outside the window bounds
        if (bulletPos.x < 0 || bulletPos.x > static_cast<float>(WINDOW_WIDTH) ||
            bulletPos.y < 0 || bulletPos.y > static_cast<float>(WINDOW_HEIGHT)) {
            isAlive = false;
        }

    }

    void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
        target.draw(shape, states);
    }

    sf::CircleShape shape;
    sf::Vector2f velocity;
    float lifetime;
    bool isAlive;
};

class Spaceship : public sf::Drawable {
public:
    Spaceship(const sf::Texture& spaceshipTexture, sf::Vector2f position)
        : mSpaceshipSprite(spaceshipTexture),
          mSpaceshipHitbox(SPACESHIP_HITBOX_RADIUS),
          mSpeed{SPACESHIP_SPEED} {
        // Calculate scaling to make the largest dimension 35px
        sf::Vector2u textureSize = spaceshipTexture.getSize();
        float scaleX = 35.0f / static_cast<float>(textureSize.x);
        float scaleY = 35.0f / static_cast<float>(textureSize.y);
        float scaleFactor = std::min(scaleX, scaleY);
        mSpaceshipSprite.setScale(sf::Vector2f(scaleFactor, scaleFactor));

        // Set origin to center of the (unscaled) texture for rotation
        mSpaceshipSprite.setOrigin(sf::Vector2f(static_cast<float>(textureSize.x) / 2.0f,
                                                static_cast<float>(textureSize.y) / 2.0f));

        // Position the spaceship
        mSpaceshipSprite.setPosition(position);

        // Create spaceship hitbox circle
        mSpaceshipHitbox.setFillColor(sf::Color(255, 255, 0, 80));  // Semi-transparent yellow
        mSpaceshipHitbox.setOutlineThickness(2.0f);
        mSpaceshipHitbox.setOutlineColor(sf::Color(200, 200, 0));  // Darker yellow border
        mSpaceshipHitbox.setOrigin(
            sf::Vector2f(SPACESHIP_HITBOX_RADIUS, SPACESHIP_HITBOX_RADIUS));  // Center origin
        mSpaceshipHitbox.setPosition(mSpaceshipSprite.getPosition());
    }

    float hitboxRadius() const { return mSpaceshipHitbox.getRadius(); }

    float speed() const { return mSpeed; }

    void setPosition(sf::Vector2f newPos) {
        mSpaceshipSprite.setPosition(newPos);
        mSpaceshipHitbox.setPosition(newPos);
    }
    sf::Vector2f getPosition() const { return mSpaceshipSprite.getPosition(); }

    void setRotation(sf::Angle angle) { mSpaceshipSprite.setRotation(angle); }
    sf::Angle getRotation() { return mSpaceshipSprite.getRotation(); }

private:
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
        target.draw(mSpaceshipHitbox, states);
        target.draw(mSpaceshipSprite, states);
    }

    sf::Sprite mSpaceshipSprite;
    sf::CircleShape mSpaceshipHitbox;
    float mSpeed;
};

class GameState : public sf::Drawable {
public:
    GameState(const ResourceManager& resources)
        : mSpaceship(resources.spaceshipTexture, WINDOW_MIDDLE),
          mExplosionSound(resources.explosionSoundBuffer),
          mRng(0),
          mAsteroidDistributionX(0.f, WINDOW_WIDTH),
          mAsteroidDistributionY(0.f, WINDOW_HEIGHT),
          mAsteroidDistributionVel(-0.7, 0.7) {
        reset();
    }

    // resets world objects, not necessarily rng state, for the purposes of this lab.
    void reset(int nAsteroids = 8) {
        mSpaceship.setPosition(WINDOW_MIDDLE);
        mAsteroids.clear();
        mBullets.clear();
        generateAsteroids(nAsteroids);
    }

    void update(const InputSummary& inputSummary) {
        // --- Update Spaceship ---
        mSpaceship.setPosition(mSpaceship.getPosition() +
                               inputSummary.movementDirection * mSpaceship.speed());
        sf::Vector2f facingVector = (inputSummary.targetPosition - mSpaceship.getPosition());
        float angleRadians = std::atan2(facingVector.y, facingVector.x);
        // Set rotation as mouse angle + 90 degrees (pi/2 radians) because
        //  1. our texture has the top of the ship at 90 degrees from the x axis
        //  2. SFML's coordinate system has y axis pointing down.
        mSpaceship.setRotation(sf::radians(angleRadians + M_PI / 2));
        // --- Shooting ---
        // =====
        // TODO: Implement shooting mechanics, keeping in mind the shooting cooldown. In detail:
        //  - Consider whether the user wants to shoot, and also the cooldown.
        //  - Bullet direction is the same as the spaceship's facing direction.
        //  - Bullet should be shot from the current spaceship position.
        if (inputSummary.shootingDesired){
            if (mShootClock.getElapsedTime().asSeconds() >= SHOOT_COOLDOWN){
                // normalize the facing vector to get the direction of the bullet
                sf::Vector2f bulletDirection = facingVector.normalized();
                sf::Vector2f bulletVelocity = bulletDirection * BULLET_SPEED;

                // add a new bullet to the bullets vector
                mBullets.emplace_back(mSpaceship.getPosition(), bulletVelocity);
                mShootClock.restart();
            }
        }

        // --- Update Asteroids ---
        for (auto& asteroid : mAsteroids) {
            asteroid.update();
        }
        // --- Update Bullets ---
        for (auto& bullet : mBullets) {
            bullet.update();
        }
        // --- Mark collided bullets/asteroids dead ---
        processCollisions();
        // --- Clean up dead bullets/asteroids ---
        cleanup();
    }

    bool gameFinished() const {
        // Close game when there are no more asteroids
        return mAsteroids.empty();
    }

private:
    void generateAsteroids(int nAsteroids) {
        for (int i = 0; i < 8; ++i) {
            mAsteroids.emplace_back(
                sf::Vector2f(mAsteroidDistributionX(mRng), mAsteroidDistributionY(mRng)),
                sf::Vector2f(mAsteroidDistributionVel(mRng), mAsteroidDistributionVel(mRng)),
                ASTEROID_RADIUS  // Initial radius (large asteroid)
            );
        }
    }

    void processCollisions() {
        // --- Bullet-Asteroid Collision Detection ---
        processCollisionsBulletAsteroid();
        // --- Spaceship-Asteroid Collision Detection ---
        processCollisionsSpaceshipAsteroid();
    }

    void processCollisionsBulletAsteroid() {
        for (auto& bullet : mBullets) {
            if (!bullet.isAlive) continue;
            for (auto& asteroid : mAsteroids) {
                if (!asteroid.isAlive) continue;
                // Check if bullet circle intersects with asteroid circle
                if (circlesIntersect(bullet.shape.getPosition(), bullet.shape.getRadius(),
                                     asteroid.shape.getPosition(), asteroid.shape.getRadius())) {
                    bullet.isAlive = false;
                    asteroid.isAlive = false;
                    // TODO: Add Explosion Sound Effect
                    // Play explosion sound!
                    mExplosionSound.play();
                    break;  // Bullet can only hit one asteroid
                }
            }
        }
    }

    void processCollisionsSpaceshipAsteroid() {
        for (auto& asteroid : mAsteroids) {
            if (!asteroid.isAlive) continue;
            // =====
            // TODO: Use Circle-Circle intersection test (circlesIntersect)
            // to determine if the spaceship's hitbox collides with an asteroid.
            // If so, kill the asteroid and play an explosion sound.
        }
    }

    void cleanup() {
        // --- Cleanup (Remove destroyed asteroids) ---
        cleanupDeadAsteroids();
        // --- Cleanup (Remove dead bullets) ---
        cleanupDeadBullets();
    }

    void cleanupDeadAsteroids() {
        std::vector<Asteroid> aux;
        aux.reserve(mAsteroids.size());
        for (const auto& asteroid : mAsteroids) {
            if (asteroid.isAlive) {
                aux.push_back(asteroid);
            }
        }
        aux.swap(mAsteroids);
    }

    void cleanupDeadBullets() {
        // =====
        // TODO: What should we do with dead bullet objects? Just keep them lying around taking up
        // space in memory?
    }

    void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
        for (const auto& asteroid : mAsteroids) {
            target.draw(asteroid, states);
        }
        for (const auto& bullet : mBullets) {
            target.draw(bullet, states);
        }
        target.draw(mSpaceship, states);
    }

    // World objects
    Spaceship mSpaceship;
    std::vector<Asteroid> mAsteroids;
    std::vector<Bullet> mBullets;
    // Shooting cooldown
    sf::Clock mShootClock;
    // Audio
    sf::Sound mExplosionSound;
    // Pseudo-random number generator state
    std::mt19937 mRng;
    // RNG distributions for asteroid position X, Y, and then velocity:
    std::uniform_real_distribution<float> mAsteroidDistributionX;
    std::uniform_real_distribution<float> mAsteroidDistributionY;
    std::uniform_real_distribution<float> mAsteroidDistributionVel;
};

int main() {
    // Create the SFML window
    sf::RenderWindow window(sf::VideoMode(sf::Vector2u(WINDOW_WIDTH, WINDOW_HEIGHT)), "Asteroids");
    window.setFramerateLimit(60);
    // Prevent key repeats.
    window.setKeyRepeatEnabled(false);
    // Load resources:
    ResourceManager resources{};
    // Load the spaceship texture
    if (!resources.spaceshipTexture.loadFromFile("assets/spaceship.png")) {
        std::cerr << "Error: Could not load image into texture." << std::endl;
    }
    // Load the explosion sound
    if (!resources.explosionSoundBuffer.loadFromFile("assets/explosion.wav")) {
        std::cerr << "Error: Could not load explosion.wav sound file." << std::endl;
    }
    GameState gameState(resources);
    // Game loop
    while (window.isOpen()) {
        // handle inputs
        bool shouldClose = false;
        InputSummary inputSummary = processInputs(window, shouldClose);
        if (shouldClose) break;
        // update
        gameState.update(inputSummary);
        if (gameState.gameFinished()) {
            window.close();
            break;
        }
        // render
        window.clear(sf::Color::Black);  // Clear the window
        window.draw(gameState);          // render everything
        window.display();                // show rendered image to screen.
    }

    return 0;
}
