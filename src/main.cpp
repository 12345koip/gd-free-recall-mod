#include <Geode/Geode.hpp>
#include <Geode/modify/DailyRecallUnlockPopup.hpp>
#include <Geode/modify/LevelCell.hpp>
#include <Geode/modify/LevelBrowserLayer.hpp>

using namespace geode::prelude;

const char* getLevelTypeStr(const int levelStatusDeterminer) {
    if (levelStatusDeterminer <= 100000)
        return "Daily";
    else if (levelStatusDeterminer <= 200000)
        return "Weekly";
    else
        return "Event";
}

const char* getExtraStringForLevelType(int levelStatusDeterminer) {
    if (levelStatusDeterminer > 100000) //weekly demon or event.
        return "You get <cy>stars</c> and the <cg>chest</c>, nothing else.";
    else
        return "You get <cy>stars</c> only. No diamonds, no bonus.";
}

class $modify(DailyRecallUnlockPopup) {
    bool init(FLAlertLayerProtocol* protocol, GJGameLevel* level) {
        if (!DailyRecallUnlockPopup::init(protocol, level))
            return false;

        const auto dailyID = level->m_dailyID.value();
        const auto levelStr = getLevelTypeStr(dailyID);
        const auto extraStr = getExtraStringForLevelType(dailyID);
        const auto text = fmt::format("Unlock this {} level for <cy>0</c> <cj>Demon Keys</c>?\n{}", levelStr, extraStr);

        for (auto* child: this->m_mainLayer->getChildrenExt<CCNode>()) {
            auto* area = typeinfo_cast<TextArea*>(child);
            if (!area) continue;

            area->setString(text);

            //setString resets the colours, need to reapply shadow
            if (area->getZOrder() == 2) { //the shadow text.
                auto* mbf = area->getChildByIndex(0); //dw this is safe its the only child of the TextArea
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

        auto* recallLabelCostContainer = this->m_button->getNormalImage()->getChildByType<CCSprite>(0);
        if (!recallLabelCostContainer) return;

        auto* costLabel = recallLabelCostContainer->getChildByType<CCLabelBMFont>(0);
        costLabel->setString("0");
    }
};

class $modify(LevelBrowserLayer) {
    //basically just reimplemented the daily recall branch to avoid the cost check
    bool cellPerformedAction(TableViewCell* rawCell, int listType, CellAction action, CCNode* parent) override {
        if (listType == 0x14 || action != static_cast<CellAction>(0x9)) //TODO: update to cell action when its added to the enum
            return LevelBrowserLayer::cellPerformedAction(rawCell, listType, action, parent);

        auto* cell = static_cast<LevelCell*>(rawCell);
        auto* copiedLevel = cell->recallLevel();

        copiedLevel->retain();
        CC_SAFE_RELEASE(this->m_recalledLevel);
        this->m_recalledLevel = copiedLevel;

        auto popup = DailyRecallUnlockPopup::create(this, copiedLevel);
        if (popup) {
            popup->setTag(7); //i cannot stress this enough im only doing this because its a reimplementation
            popup->show();
        }

        return true;
    }

    void FLAlert_Clicked(FLAlertLayer* layer, bool btn2) override {
        if (layer->getTag() != 7 || !btn2)
            return LevelBrowserLayer::FLAlert_Clicked(layer, btn2);

        auto* level = this->m_recalledLevel;
        auto cost = GameStatsManager::dailyRecallPrice(level->m_dailyID.value());
        auto* StatsManager = GameStatsManager::sharedState();
        StatsManager->incrementStat("21", cost);

        LevelBrowserLayer::FLAlert_Clicked(layer, btn2);

        //need to give a reward so this persists
        auto obj = GJRewardObject::create(SpecialRewardItem::BonusKey, cost, 0);
        auto item = GJRewardItem::createWithObject(GJRewardType::SmallTreasure, obj);
        StatsManager->m_weeklyChest->setObject(item, fmt::format("x_freerecall_{}", level->m_dailyID.value()));
    }
};