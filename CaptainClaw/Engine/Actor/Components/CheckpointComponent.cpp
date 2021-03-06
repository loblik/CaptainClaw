#include "CheckpointComponent.h"
#include "../../GameApp/BaseGameApp.h"
#include "../../GameApp/BaseGameLogic.h"

#include "PositionComponent.h"
#include "AnimationComponent.h"
#include "PhysicsComponent.h"

#include "../../Events/EventMgr.h"
#include "../../Events/Events.h"

const char* CheckpointComponent::g_Name = "CheckpointComponent";

CheckpointComponent::CheckpointComponent()
    :
    m_SpawnPosition(Point(0, 0)),
    m_IsSaveCheckpoint(false),
    m_SaveCheckpointNumber(0)
{ }

bool CheckpointComponent::VDelegateInit(TiXmlElement* pData)
{
    assert(pData);

    if (TiXmlElement* pElem = pData->FirstChildElement("SpawnPosition"))
    {
        pElem->Attribute("x", &m_SpawnPosition.x);
        pElem->Attribute("y", &m_SpawnPosition.y);
    }
    if (TiXmlElement* pElem = pData->FirstChildElement("IsSaveCheckpoint"))
    {
        m_IsSaveCheckpoint = std::string(pElem->GetText()) == "true";
    }
    if (TiXmlElement* pElem = pData->FirstChildElement("SaveCheckpointNumber"))
    {
        m_SaveCheckpointNumber = std::stoi(pElem->GetText());
    }

    assert(!m_SpawnPosition.IsZero());

    return true;
}

void CheckpointComponent::VCreateInheritedXmlElements(TiXmlElement* pBaseElement)
{

}

bool CheckpointComponent::VOnApply(Actor* pActorWhoPickedThis)
{
    shared_ptr<AnimationComponent> pAnimationComponent =
        MakeStrongPtr(_owner->GetComponent<AnimationComponent>(AnimationComponent::g_Name));
    assert(pAnimationComponent);
    pAnimationComponent->ResumeAnimation();
    pAnimationComponent->AddObserver(this);

    shared_ptr<PhysicsComponent> pPhysicsComponent =
        MakeStrongPtr(_owner->GetComponent<PhysicsComponent>(PhysicsComponent::g_Name));
    assert(pPhysicsComponent);
    pPhysicsComponent->Destroy();

    shared_ptr<EventData_Checkpoint_Reached> pEvent(new EventData_Checkpoint_Reached(pActorWhoPickedThis->GetGUID(), m_SpawnPosition, m_IsSaveCheckpoint, m_SaveCheckpointNumber));
    IEventMgr::Get()->VQueueEvent(pEvent);

    // I Need to specify this here since I return false
    IEventMgr::Get()->VTriggerEvent(IEventDataPtr(new EventData_Request_Play_Sound(SOUND_GAME_FLAG_RISE, 100, false)));

    return false;
}

void CheckpointComponent::VOnAnimationAtLastFrame(Animation* pAnimation)
{
    if (!(pAnimation->GetName() == "wave"))
    {
        shared_ptr<AnimationComponent> pAnimationComponent =
            MakeStrongPtr(_owner->GetComponent<AnimationComponent>(AnimationComponent::g_Name));
        assert(pAnimationComponent);

        assert(pAnimationComponent->SetAnimation("wave"));

        IEventMgr::Get()->VTriggerEvent(IEventDataPtr(new EventData_Request_Play_Sound(SOUND_GAME_FLAG_WAVE, 100, false)));
    }
}