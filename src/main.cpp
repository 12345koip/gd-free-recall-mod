#include <Geode/Geode.hpp>
#include <Geode/modify/DailyRecallUnlockPopup.hpp>
#include <Geode/modify/LevelCell.hpp>
#include <Geode/modify/LevelBrowserLayer.hpp>

using namespace geode::prelude;

int getKeyCost(int levelStatusDeterminer) {
    int numKeysRequired = 0;
    if (levelStatusDeterminer <= 100000) //daily
        numKeysRequired = 1;
    else if (levelStatusDeterminer <= 200000) //weekly
        numKeysRequired = 3;
    else //event
        numKeysRequired = 5;

    return numKeysRequired;
}

const char* getLevelTypeStr(int levelStatusDeterminer) {
    switch (getKeyCost(levelStatusDeterminer)) {
        case 1:
            return "Daily";
        case 3:
            return "Weekly";
        case 5:
            return "Event";
        default:
            return "Unknown";
    }
}

const char* getExtraStringForLevelType(int levelStatusDeterminer) {
    if (levelStatusDeterminer > 100000) //weekly demon or event.
        return "You get <cy>stars</c> and the <cg>chest</c>, nothing else.";
    else
        return "You get <cy>stars</c> only. No diamonds, no bonus.";
}

class $modify(DailyRecallUnlockPopup) {
    bool init(FLAlertLayerProtocol* protocol, GJGameLevel* level) {
        bool initResult = DailyRecallUnlockPopup::init(protocol, level);
        if (!initResult) return false;

        auto statusID = level->m_dailyID.value();
        const auto levelStr = getLevelTypeStr(statusID);
        const auto extraStr = getExtraStringForLevelType(statusID);
        const auto text = fmt::format("Unlock this {} level for <cy>0</c> <cj>Demon Keys</c>?\n{}", levelStr, extraStr);

        for (auto* child: this->m_mainLayer->getChildrenExt<CCNode>()) {
            auto* area = typeinfo_cast<TextArea*>(child);
            if (!area) continue;

            area->setString(text);

            //setString resets the colours, need to reapply shadow
            if (area->getZOrder() == 2) { //the shadow text.
                auto* mbf = area->getChildByType<MultilineBitmapFont>(0);
                if (!mbf) continue;

                for (auto* line: mbf->getChildrenExt<CCLabelBMFont>())
                    line->setColor({0, 0, 0});

                area->setOpacity(100);
            }
        }

        return true;
    }
};

class $modify(LevelCell) {
    void loadCustomLevelCell() {
        LevelCell::loadCustomLevelCell();

        auto* item = typeinfo_cast<CCMenuItemSpriteExtra*>(this->m_mainMenu->getChildByTag(5));
        if (!item) return;

        auto* button = typeinfo_cast<ButtonSprite*>(item->getNormalImage());
        if (!button) return;

        auto* container = button->getChildByType<CCSprite>(0);
        if (!container) return;

        auto* costLabel = container->getChildByType<CCLabelBMFont>(0);
        costLabel->setString("0");
    }
};

class $modify(LevelBrowserLayer) {
    //basically just reimplemented the daily recall branch to avoid the cost check
    bool cellPerformedAction(TableViewCell* rawCell, int listType, CellAction action, CCNode* parent) override {
        if (listType == 0x14 || action != static_cast<CellAction>(0x9)) //TODO: update to cell action when its added to the enum
            return LevelBrowserLayer::cellPerformedAction(rawCell, listType, action, parent);

        auto* cell = static_cast<LevelCell*>(rawCell);
        auto* originalLevel = cell->m_level;
        auto* copiedLevel = originalLevel->duplicate();
        copiedLevel->m_dailyID = originalLevel->m_replayDailyID;

        copiedLevel->retain();
        CC_SAFE_RELEASE(this->m_recalledLevel);
        this->m_recalledLevel = copiedLevel;

        auto popup = DailyRecallUnlockPopup::create(this, copiedLevel);
        if (popup) {
            popup->setTag(7);
            popup->show();
        }

        return true;
    }

    void FLAlert_Clicked(FLAlertLayer* layer, bool btn2) override {
        auto* level = this->m_recalledLevel;
        if (layer->getTag() != 7 || !btn2)
            return LevelBrowserLayer::FLAlert_Clicked(layer, btn2);

        auto cost = getKeyCost(level->m_dailyID.value());
        auto* StatsManager = GameStatsManager::sharedState();
        StatsManager->incrementStat("21", cost);

        LevelBrowserLayer::FLAlert_Clicked(layer, btn2);

        //need to give a reward so this persists
        auto obj = GJRewardObject::create(SpecialRewardItem::BonusKey, cost, 0);
        auto item = GJRewardItem::createWithObject(GJRewardType::SmallTreasure, obj);
        StatsManager->m_weeklyChest->setObject(item, fmt::format("x_freerecall_{}", level->m_dailyID.value()));
    }
};