#ifndef __ACTOR_TEMPLATES_H__
#define __ACTOR_TEMPLATES_H__

#include "../SharedDefines.h"

// Structs for creating components
struct BaseAuraComponentDef
{
    BaseAuraComponentDef()
    {
        isPulsating = false;
        isGroupPulse = false;
        applyAuraOnEnter = true;
        removeActorAfterPulse = false;
        pulseIntrval = 0;
    }

    ActorFixtureDef auraFixtureDef;
    bool isPulsating;
    bool isGroupPulse;
    bool applyAuraOnEnter;
    bool removeActorAfterPulse;
    int pulseIntrval;
};

struct DamageAuraComponentDef
{
    DamageAuraComponentDef()
    {
        damage = 0;
    }

    BaseAuraComponentDef baseAuraComponentDef;
    int damage;
};

namespace ActorTemplates
{
    // Exposed Component Xml data generating functions
    TiXmlElement* CreateXmlData_GlitterComponent(std::string glitterType, bool spawnImmediate, bool followOwner);
    TiXmlElement* CreatePhysicsComponent(const ActorBodyDef* pBodyDef);
    TiXmlElement* CreatePredefinedMoveComponent(std::vector<PredefinedMove>& moves, bool isInfinite);
    TiXmlElement* CreateFollowableComponent(Point offset, std::string imageSet, std::string animPath);

    TiXmlElement* CreateDamageAuraComponent(DamageAuraComponentDef aurDef);

    // Exposed Actor Xml data generating functions.
    TiXmlElement* CreateXmlData_CrateActor(std::string imageSet, Point position, const std::vector<PickupType>& loot, uint32 health, int32 zCoord);
    TiXmlElement* CreateXmlData_PowderKegActor(std::string imageSet, Point position, int32 damage, int32 zCoord);
    TiXmlElement* CreateXmlData_CrumblingPeg(std::string imageSet, Point position, int32 zCoord);
    TiXmlElement* CreateXmlData_GlitterActor(std::string glitterType, Point position, int32 zCoord);
    TiXmlElement* CreateXmlData_CheckpointActor(std::string imageSet, Point position, int32 zCoord, Point spawnPosition, bool isSaveCheckpoint, uint32 saveCheckpointNumber);
    TiXmlElement* CreateXmlData_EnemyAIActor(std::string imageSet, std::string animationSet, Point position, const std::vector<PickupType>& loot, std::string logicName, int32 zCoord, int32 minPatrolX, int32 maxPatrolX);
    TiXmlElement* CreateXmlData_AmmoPickupActor(std::string imageSet, std::string pickupSound, Point position, bool isStatic);
    TiXmlElement* CreateXmlData_LifePickupActor(std::string imageSet, std::string pickupSound, Point position, bool isStatic);
    TiXmlElement* CreateXmlData_TreasurePickupActor(std::string imageSet, std::string pickupSound, Point position, bool isStatic);
    TiXmlElement* CreateXmlData_PowerupPickupActor(std::string imageSet, std::string pickupSound, Point position, bool isStatic);
    TiXmlElement* CreateXmlData_HealthPickupActor(std::string imageSet, std::string pickupSound, Point position, bool isStatic);

    TiXmlElement* CreateXmlData_SoundTriggerActor(const std::string& sound, Point position, Point size, int enterCount, bool activateDialog = false);
    TiXmlElement* CreateXmlData_SoundTriggerActor(const std::string& sound, const std::string& logicName, Point position, SDL_Rect presetPosition, int enterCount);

    TiXmlElement* CreateXmlData_GlobalAmbientSoundActor(const std::string& sound, int soundVolume, int minTimeOff, int maxTimeOff, int minTimeOn, int maxTimeOn, bool isLooping);

    // This is used by the game itself
    StrongActorPtr CreateActorPickup(PickupType pickupType, Point position, bool isStatic = false);
    StrongActorPtr CreatePowerupSparkleActor();
    StrongActorPtr CreateClawProjectile(AmmoType ammoType, Direction direction, Point position);
    StrongActorPtr CreateProjectile(std::string imageSet, uint32 damage, DamageType damageType, Direction direction, Point position, CollisionFlag collisionFlag, uint32 collisionMask);
    StrongActorPtr CreateAreaDamage(Point position, Point size, int32 damage, CollisionFlag collisionFlag, std::string shape, DamageType damageType, Direction hitDirection, Point positionOffset = Point(0, 0), std::string imageSet = "", int32 zCoord = 0);
    StrongActorPtr CreateGlitter(std::string glitterType, Point position, int32 zCoord = 1010);
    //StrongActorPtr CreatePopupActor(Point position, std::string imageSet, std::vector<PredefinedMove>& moves, int32 zCoord = 0);
    StrongActorPtr CreateScorePopupActor(Point position, int score);
    StrongActorPtr CreateRenderedActor(Point position, std::string imageSet, std::string animPath, int zCoord);

    // This represents (or atleast should represent) single animation, e.g. explosion
    StrongActorPtr CreateSingleAnimation(Point position, AnimationType animType);

    ActorFixtureDef XmlToActorFixtureDef(TiXmlElement* pActorFixtureDefElem);
    TiXmlElement* ActorFixtureDefToXml(const ActorFixtureDef* pFixtureDef);

    TiXmlElement* DamageAuraComponentDefToXml(const DamageAuraComponentDef* pAuraDef);
};

#endif
