# API flow model — the wire RPC surface (`SetLuaNetFunc`)
Auto-generated from `Res/tw_lua_decompiled` (regenerate: `ghidraRE/output/gen_api_surface.py`). Every replicated method a Lua object exposes on the wire is registered via `self:SetLuaNetFunc(name, RPCType.*)`. **Client** = client-invokable (server->client downcall), **Server** = server-invokable (client->server upcall), **Master** = master-channel. Native surface the Lua rides on = `native_api.md`; the login->roster->char call graph = `../client/_MASTER.md`.
- **1252** registrations across **64** owning classes
- RPCType split: Client=569, Server=565, Master=139

## Owning classes (by RPC count)
| class | file | RPCs | server counterpart |
|---|---|---:|---|
| `CPrivateStatus` | `Class/Main/CPrivateStatus.lua` | 158 | — |
| `CToolInterface` | `Class/Main/CToolInterface.lua` | 120 | — |
| `Player` | `Class/Main/Player.lua` | 119 | server/src/char/ |
| `PartyProxy` | `Class/Main/Party/PartyProxy.lua` | 92 | — |
| `CItemManager` | `Class/Main/Item/CItemManager.lua` | 71 | — |
| `Channel` | `Class/Main/Channel.lua` | 66 | — |
| `QuestManager` | `Class/Main/Quest/QuestManager.lua` | 60 | — |
| `GuildProxy` | `Class/Main/Community/GuildProxy.lua` | 55 | — |
| `ActorController` | `Class/Main/ActorController.lua` | 53 | — |
| `PlayerController` | `Class/Main/PlayerController.lua` | 34 | — |
| `CCommunity` | `Class/Main/Community/CCommunity.lua` | 34 | — |
| `AIController` | `Class/Main/AI/AIController.lua` | 33 | — |
| `Exchanger` | `Class/Main/Item/Exchanger.lua` | 29 | — |
| `User` | `Class/Main/User.lua` | 23 | server/src/char/ |
| `ZoneBossInstanceManager` | `Class/Main/ZoneBossInstanceManager.lua` | 22 | — |
| `CChat` | `Class/Main/CChat.lua` | 20 | — |
| `Party` | `Class/Main/Party/Party.lua` | 20 | — |
| `PersonalStore` | `Class/Main/Item/PersonalStore.lua` | 18 | — |
| `CMarket` | `Class/Main/CMarket.lua` | 17 | — |
| `CStatus` | `Class/Main/CStatus.lua` | 16 | server/src/char/ |
| `PlayerController_Fishing` | `Class/Main/PlayerController_Fishing.lua` | 15 | — |
| `CMailBoxEx` | `Class/Main/Community/CMailBoxEx.lua` | 15 | — |
| `CStatusPlayer` | `Class/Main/CStatusPlayer.lua` | 14 | server/src/char/ |
| `CEquipment` | `Class/Main/Item/CEquipment.lua` | 13 | — |
| `NpcControllerEx` | `Class/Main/NpcControllerEx.lua` | 11 | — |
| `CSkillBook` | `Class/Main/Skill/CSkillBook.lua` | 10 | — |
| `CInventory` | `Class/Main/Item/CInventory.lua` | 8 | — |
| `CPrivateStatus_HotKeySettings` | `Class/Main/CPrivateStatus_HotKeySettings.lua` | 7 | — |
| `Trigger` | `Class/Main/Trigger.lua` | 7 | — |
| `CashProductList` | `Class/Main/CashProduct/CashProductList.lua` | 7 | — |
| `CGuild` | `Class/Main/Community/CGuild.lua` | 7 | — |
| `CStatusItem` | `Class/Main/Item/CStatusItem.lua` | 6 | — |
| `TriggerCommands` | `Class/Main/TriggerCommands.lua` | 5 | — |
| `AIController_Ability` | `Class/Main/AI/AIController_Ability.lua` | 5 | — |
| `CItemContainer` | `Class/Main/Item/CItemContainer.lua` | 5 | — |
| `PetController` | `Class/Main/Pet/PetController.lua` | 5 | — |
| `PetStatus` | `Class/Main/Pet/PetStatus.lua` | 5 | — |
| `Session` | `Class/Main/Session.lua` | 4 | server/src/net/ (login) |
| `Component` | `Lib/Component.lua` | 4 | — |
| `CBody` | `Class/Main/CBody.lua` | 3 | — |
| `CBombController` | `Class/Main/CBombController.lua` | 3 | — |
| `Interacter` | `Class/Main/Interacter.lua` | 3 | — |
| `CJournal` | `Class/Main/Community/CJournal.lua` | 3 | — |
| `DynamicObjectController` | `Class/Main/DynamicObjectController.lua` | 2 | — |
| `NpcInteracter` | `Class/Main/NpcInteracter.lua` | 2 | — |
| `CItemController` | `Class/Main/Item/CItemController.lua` | 2 | — |
| `PartyList` | `Class/Main/Party/PartyList.lua` | 2 | — |
| `Quest` | `Class/Main/Quest/Quest.lua` | 2 | — |
| `CAdminClient` | `ClassEx/ClientOnly/CAdminClient.lua` | 2 | — |
| `CFieldController` | `Class/Main/CFieldController.lua` | 1 | — |
| `CPrivateStatus_Fishing` | `Class/Main/CPrivateStatus_Fishing.lua` | 1 | — |
| `CTrigger` | `Class/Main/CTrigger.lua` | 1 | — |
| `Physics` | `Class/Main/Physics.lua` | 1 | — |
| `CAccountBank` | `Class/Main/Item/CAccountBank.lua` | 1 | — |
| `Controller` | `Lib/Com/Controller.lua` | 1 | — |
| `TSkill_Magician_ChainLightning` | `Object/Main/Skill/Magician/TSkill_Magician_ChainLightning.lua` | 1 | — |
| `TSkill_Magician_Lightning` | `Object/Main/Skill/Magician/TSkill_Magician_Lightning.lua` | 1 | — |
| `TSkill_Magician_LightningCurse` | `Object/Main/Skill/Magician/TSkill_Magician_LightningCurse.lua` | 1 | — |
| `TSkill_Magician_LightningSpark` | `Object/Main/Skill/Magician/TSkill_Magician_LightningSpark.lua` | 1 | — |
| `TSkill_Magician_LightningThunderFog` | `Object/Main/Skill/Magician/TSkill_Magician_LightningThunderFog.lua` | 1 | — |
| `TSkill_Magician_MovingLightning` | `Object/Main/Skill/Magician/TSkill_Magician_MovingLightning.lua` | 1 | — |
| `TSkill_Magician_ContinuousLightning` | `Object/Main/Skill/Magician/LightningMagician/TSkill_Magician_ContinuousLightning.lua` | 1 | — |
| `TSkill_Monster_Lightning` | `Object/Main/Skill/Monster/TSkill_Monster_Lightning.lua` | 1 | — |
| `TSkill_Monster_LightningCurse_All` | `Object/Main/Skill/Monster/TSkill_Monster_LightningCurse_All.lua` | 1 | — |

## Per-class RPC listing

### `CPrivateStatus`  (158 RPCs) — `Class/Main/CPrivateStatus.lua`
- **?** (1): `OnServerMSSpawnEven`
- **Client** (83): `AddMapChangedJob`, `AttachRideSlot`, `CallUIPopUp`, `CancelStartBattleResult`, `CancelStartBattleResult_Compensation`, `CannotCancelArenaMoveDialog`, `Client_DoOpenPersonalStore`, `CloseThemeBossKeyUI`, `ConfirmExitInstance`, `ConfirmExitInstanceDelay`, `ForceStartGoddessSkill`, `HideArenaMoveDialog`, `HideNpcDialog`, `IncreaseMineExpResult`, `IncreaseMineLevelResult`, `InitRebirthCount`, `NewShowSystemMsg`, `NotifyAddQuestItem`, `NotifyMarbleGaugeMax`, `OnCheckGoddessPortal`, `OnCheckGoddessSkill`, `OnClearRevengeInfo`, `OnClearRevengeInfoMember`, `OnClientNotifyEventProcess`, `OnGivePCRm`, `OnGiveVIP`, `OnMoveProcess`, `OnResultRemoveVehicle`, `OnResultRideBegin`, `OnResultRideEnd`, `OnShowClearUI`, `OnShowFightUIArena`, `OnShowFightUISpecialIns`, `OnSoundProcess`, `OnSuccessRevenge`, `OnSuccessRevengeMember`, `OnSurrenderRevenge`, `OnSurrenderRevengeMember`, `OutCallUIPopUp`, `PopupMagicSquareConfirmHide`, `RequestCallUIPopUp`, `ResultCheckStatPerHours`, `ResultMessageBlessedTarget`, `ResultMessageBlesser`, `ResultMessageCursedTarget`, `ResultMessageCurser`, `SetInstancePvP_changescore`, `SetInstancePvP_initscore`, `SetInstancePvP_leave`, `SetInstancePvP_start`, `ShowArenaMoveDialog`, `ShowArenaMoveEffect`, `ShowCancelInstancePvPDlg`, `ShowEnterInstancePvPDlg`, `ShowFailMatchPvPDlg`, `ShowFinishEmotionPvP`, `ShowFinishInstancePvPDlg`, `ShowInsFailDialog`, `ShowInsRetryDialog`, `ShowInstancePvP_scoreBoard`, `ShowInventory`, `ShowNotifyUI`, `ShowPopup`, `ShowRewardDialog`, `ShowSystemMsg`, `ShowThemeBossKeyUI`, `ShowWaitInstancePvPDlg`, `ShowZoneGauge`, `StartMagicSquare`, `StoryHandleEvent`, `SyncCoolTime`, `SyncSavedCoolTime`, `UpdateFieldFatigueResult`, `UpdateThemeBossKeyUI`, `ZoneMoveNpcResultFail`, `ZoneMoveNpcResultSuccess`, `onCoolTimeItemUsed`, `onIsPCRoom`, `onIsVIP`, `onRoomPlayerCameraShake`, `onSkillStartNotify`, `onSummonPartyPlayerInThemeBoss`, `onTimeLimitInsFailProcess`
- **Client+Server** (3): `ResetVehicles`, `SetFatigue`, `SetFightInForbidSkillMap`
- **Master** (3): `OnMasterAddRefuseList`, `OnMasterDelRefuseList`, `OnMasterInitRefuseList`
- **Server** (65): `AddDibs`, `CancelRegistInstancePvP`, `CheckGoddessSkill`, `CheckGoddessSkillPortal`, `CheckInsAliveFromClient`, `ClearInstanceInfo`, `ClearZoneNext`, `CommandExitInstance`, `CompleteMarbleStoneQuest`, `ConfirmInstancePvP`, `CreateMapPortalProcess`, `CreateServerObject`, `CreateSpecialInstancePortalProcess`, `CreateTownPortalProcess`, `CreateZoneEndPortalProcess`, `DeleteGoddessSkill`, `FatigueFloatUp`, `GetRoomName`, `GetThemeBossKey`, `IncreaseFavorStat`, `IncreaseMarbleStoneWrapper`, `IncreaseMaxGaugeCount`, `IncreaseMineLevel`, `MapObjectAction`, `MonsterDescription`, `NetIncreaseMineExp`, `OnBless`, `OnCurse`, `OnServerAddRefuseList`, `OnServerDelRefuseList`, `OnServerInitRefuseList`, `OnServerRideBegin`, `OnServerRideEnd`, `OutInsLogProcess`, `OutInstanceRoom`, `QuitInstanceClearCutScene`, `QuitInstanceStartCutScene`, `RemoveDibs`, `RequestCancelMoveArena`, `RequestCancelStartBattle`, `RequestCancelStartBattle_Compensation`, `RequestExitInsStartCutScene`, `RequestThemeBossKeyType`, `ResurrectionPointRestore`, `RetryInstanceDungeon`, `STest1`, `SetEnableGoddessSkillType`, `SetNotifyMap`, `SetSelectedMode`, `SetSpecialInsFloor`, `SetUserGuideStatus`, `SetUserGuideVar`, `SetUserGuideVisible`, `SetVisitedAreas`, `SpawnInsMonster`, `SpecialInsMapLoadingEndProcess`, `SummonPartyPlayerInThemeBoss`, `SurrenderRevenge`, `UnclearZone`, `UpdateUsePtlSkValue`, `UpdateValue_mSkIcL`, `UpdatemEnMsGgValue`, `UpdatemUGdMsgRmCntValue`, `UseGodSkillQuestUpdate`, `ZoneMoveNpc`
- **Server+Master** (3): `OnSetDialogAutoShowTime`, `SetUndying`, `SetVisitedMap`

### `CToolInterface`  (120 RPCs) — `Class/Main/CToolInterface.lua`
- **?** (94): `AckMonster`, `AddMobSkill`, `AddQuest`, `ChargeTestPoint`, `ClassUp`, `ClearZoneNext`, `ComWeeklyBingoMs`, `CreateDummyMonster`, `CreateItem`, `CreateMapAreaObject`, `CreateMonsterHere`, `DelAllMonster`, `DelMonsterByAreaName`, `DeleteMobSkill`, `DeleteMonster`, `ForceClearMission`, `ForceCompleteQuest`, `ForceResetWeeklyBingo`, `ForceResetWeeklyBingoData`, `ForceSetFatigue`, `GetCurrentMapName`, `GetRoomNameList`, `IncreasePlayerMarbleJam`, `IncreasePlayerMoney`, `IncreasePlayerStat`, `NetAddMonsterAbility`, `NetAddMonsterDeadEffect`, `NetAddMonsterDeadEndEffect`, `NetAddMonsterParts`, `NetClearInventory`, `NetClearObjectCache`, `NetCreateNewMonster`, `NetDeleteMonsterAbility`, `NetDeleteMonsterParts`, `NetDisableObjectCache`, `NetFindSpawnRules`, `NetGetMapAreaInfo`, `NetGetMapAreaList`, `NetGetMonsterIDListEx`, `NetIncreaseZonePoint`, `NetMigrationData`, `NetModifySpawnRules`, `NetMoveToPosition`, `NetRemoveMonsterDeadEffect`, `NetRemoveMonsterDeadEndEffect`, `NetRequestAllQuestInfo`, `NetSetMonsterAbilityData`, `NetSetSkillData`, `NotifyCurrentMapName`, `NotifyFindSpawnRules`, `NotifyMapAreaList`, `NotifyMonsterIDListEx`, `NotifySetPlayerLevel`, `Rebirth`, `ReloadFileCore`, `RemoveItem`, `RepairAllEquipment`, `ReplyRequestAllQuestInfo`, `ReplyRequestAllQuestInfo`, `RequestDetachBox`, `RequestSpawnRules`, `RequestStartSpawn`, `RequestStartSpawnAll`, `RequestStopSpawn`, `RequestStopSpawnAll`, `ResetQuestLimitCount`, `SaveSpawnRulesToFile`, `SetGlobalVariable`, `SetHavingDifficulty`, `SetMobAIType`, `SetMobBasicStat`, `SetMobBasicStatByLevel`, `SetMobBattleStat`, `SetMobBattleStatByLevel`, `SetMobBodyMaterial`, `SetMobCollision`, `SetMobCommonData`, `SetMobDeadEffect`, `SetMobLevelData`, `SetMobName`, `SetMobScale`, `SetMobTemplateData`, `SetMobWeapon`, `SetMobWeaponMaterial`, `SetPlayerStat`, `SetStaminaResetTime`, `SkillLearn`, `SkillLevelSet`, `SkillPointInit`, `T_SetMarbleStoneMax`, `UnclearZone`, `Update`, `UpdateGMonsterLevelData`, `UpdateSkillInfo`
- **Client** (1): `SetMapList`
- **Master** (1): `EstablishGuildForce`
- **Server** (24): `ChangeRoom`, `DetachBoxAllMonster`, `DropItem`, `ForceResetBlessCurseList`, `ForceResetCurseCount`, `InitBingoData`, `IsSetEnChantLevel`, `MoveToMap`, `PutItemEx`, `RepairAllInventory`, `SetFavorStat`, `SetGuildExpMax`, `SetPCRoom`, `SetPlayerLevelInServer`, `SetVIP`, `SpecialInsMove`, `T_IncreaseMarbleStoneCount`, `T_SetGoddessSkill`, `T_SetGoddessSkillAll`, `T_SetSkillAll`, `T_SkillLearn`, `T_SkillPointInit`, `VirtualDropTest`, `VisitAllMaps`

### `Player`  (119 RPCs) — `Class/Main/Player.lua`
- **?** (5): `GetSkillCountResult`, `MeasureRTTDown`, `MeasureRTTUp`, `PlayerDeSummonNPC`, `PlayerSummonNPC`
- **Client** (68): `AddTimeQuestClient`, `Client_OnChannelListRefresh`, `DiceResult`, `DumpItemDropTestResult`, `EffectFilterReFresh`, `HandleUIEvent`, `MonsterDescriptionResult`, `OnClientGetSRQLResult`, `OnClientShowClassUpInfoDialog`, `OnClientShowCommonSkillNotify`, `OnClientShowDialog`, `OnClientShowQuestDialog`, `OnClient_ChangeAntiFishingMessage`, `OnClient_GetSafeModeState`, `OnClient_ResultBlockPortal`, `OnClient_ResultCubeState`, `OnClientshowFinishEpisode`, `OnOpenCashShop`, `OnPayResult`, `OnReadCashResult`, `OnRequestCellData`, `OnShowAutoQuest`, `OnShowNpcDialog`, `PartsEffectReFresh`, `PlayerValidCheckResult`, `ReplyRequestAllQuestInfo`, `SetHideBankABUI`, `SetHideBankPBUI`, `ShowSendMailPopup`, `exeMissionQuest`, `netCheckClientABCache`, `netCheckClientPBCache`, `onCheckPathChange`, `onCheckPathChangeZoneStone`, `onChooseReward`, `onClientAasSystemMessage`, `onClientAfterMapChange`, `onClientgetTotalMailCount`, `onClosePartyInviteUI`, `onDeadHandler`, `onDirectingSceneStart`, `onGoGoShow`, `onInitializeSkillPoint`, `onLookForPartyReload`, `onMonsterDropItem`, `onNewSystemMessage`, `onNotice`, `onPopupMsgBox`, `onPopupMsgBoxPlain`, `onPopupUI`, `onPositionEffect`, `onQuest`, `onQuestComplete`, `onResetCameraTarget`, `onResultOneGameTime`, `onResultSchema_UpdateData`, `onScriptPage`, `onScriptPage2`, `onScripts`, `onSetCameraTarget`, `onSetChannelChangeUI`, `onSetDestination`, `onSetMessageDialog`, `onSetPlayerAttrDisableArea`, `onShowBoard`, `onSystemMessage`, `onSystemMessageWithSound`, `showUICostom`
- **Client+Server** (3): `CloseCashShop`, `OpenCashShop`, `SetDeleted`
- **Client+Server+Master** (1): `GCallFn`
- **Master** (6): `OnPlayerValidCheck`, `SetCurVehicle`, `findRcpt`, `onMasterOneGameTime`, `onMasterRequestQueryTime`, `onMasterTestUserStart`
- **Server** (31): `AddDibs`, `BuyProduct`, `CanOpenCashShop`, `ForceReplicate`, `NetRequestQuestInfo`, `OnClosePartyList`, `OnServerGetNpcPortraitInfo`, `OnServerGetSRQL`, `OnServer_AnswerClientKey`, `OnServer_AnswerKey`, `OnServer_CheckBlockCube`, `OnServer_CheckClientFile`, `OnServer_CheckInjectResult`, `OnServer_GetSafeModeState`, `OnServer_InvalidFileReport`, `ReadCash`, `RequestCellData`, `RunTutorialScript`, `Schema_UpdateData`, `callbackScript2`, `exitScript2`, `getMonsterDescription`, `nextScript2`, `onCloseAccountBank`, `onClosePrivateBank`, `onFindRcpt`, `runScript`, `runScript2`, `selectScriptResult`, `sendGift`, `showUIserver`
- **Server+Master** (5): `Master_ChannelListRefresh`, `reqAccountBank`, `reqAccountBankAndAddTab`, `reqPrivateBank`, `reqPrivateBankAndAddTab`

### `PartyProxy`  (92 RPCs) — `Class/Main/Party/PartyProxy.lua`
- **?** (1): `Dice`
- **Client** (42): `AddInvitationJob`, `DicePickupItemResult`, `DiceResult`, `GetZoneDataForCreateParty`, `OnCheckRecallStart`, `OnCheckRequestJoin`, `OnClientBroadcast`, `OnRequestPartyInfoByIDResult`, `OnSetPartyInfo`, `OnShowInviteConfirm`, `OnShowRequestConfirm`, `PartyPickupResult`, `ReplyCheckPartyPlayerState`, `RequestDicePickupItem`, `SetPickupRuleResult`, `ShowContextMenu`, `ShowContextMenuEx`, `onCancelEstablishGuildByLeave`, `onDenyRequestParty`, `onEstablishGuildConfirm`, `onEstablishGuildConfirmResult`, `onInviteAcceptEvent`, `onInviteCancelEvent`, `onInviteDenyEvent`, `onInviteRequestEvent`, `onInviteStateDenyEvent`, `onPartyChatEvent`, `onPartyConnectEvent`, `onPartyDisconnectEvent`, `onPartyFinish`, `onPartyJoinEvent`, `onPartyKickEvent`, `onPartyLeaveEvent`, `onPartyMaintainEvent`, `onPartyPromoteEvent`, `onPartyRecallCancelEvent`, `onPartyStart`, `onRecallRequestEvent`, `onRequestJoin`, `onSummonAcceptEvent`, `onSummonDenyEvent`, `onSummonRequestEvent`
- **Client+Server** (7): `LookForParty`, `OnPartyMemberCountUpdate`, `SetRequestPartyMessageResult`, `SetRequestPartyRequestResult`, `SetRequestPartyResult`, `onClearInvitations`, `onleave`
- **Master** (9): `DenyRequestParty`, `OnGetZoneDataForCreateParty`, `SetRequestParty`, `UpdatePartyBuddyList`, `chat`, `deleteInvitationsMaster`, `invite`, `inviteByPid`, `summon`
- **Server** (11): `CheckRecallStart`, `KickCheckAndMasterCall`, `OnCheckRecallStart_Server`, `PartyRecall`, `PartyRecallCancel`, `PartySendMassage`, `RequestCheckPartyPlayerState`, `RequestPartyInfoByID`, `UpdatePartyBuddyBuff`, `inviteCheckAndMasterCall`, `onSummon`
- **Server+Master** (22): `ClearGuildData`, `DenyRequestRecall`, `OpenInviteDialog`, `OpenRequestDialog`, `SetPartyInfo`, `SetPickupRule`, `checkPartyState`, `checkPartyStateInChat`, `checkRequestJoin`, `inviteRequestAccept`, `inviteRequestDeny`, `inviteStateDeny`, `inviteWaitingCancel`, `kick`, `leave`, `onAcceptRequestParty`, `onRequestGuildConfirm`, `promote`, `requestJoinConfirm`, `requestToLeader`, `summonRequestAccept`, `summonRequestDeny`

### `CItemManager`  (71 RPCs) — `Class/Main/Item/CItemManager.lua`
- **Client** (36): `AddItemToPrivateBankResult`, `AttachRelicResult`, `BeforePickupMarbleStone`, `BuyItemFromCashShopResult`, `BuyItemFromNPCResult`, `CheckBankCommisionResult`, `DepositMoneyToPrivateBankResult`, `EquipResult`, `IdentifyItemResult`, `ItemCreateSocketResult`, `ItemDestoyResult`, `ItemDisassembleResult`, `ItemFeatureAppraisalResult`, `MoveItemInContainerFail`, `OnClientGetInventoryItemCountResult`, `OnClinetContainerItemResult`, `PickupResult`, `PowerStoneDisassembleResult`, `RemoveItemFromPrivateBankResult`, `RepurcsaseItemFromNPCResult`, `SelectEquipmentPageReply`, `SellItemToNPCResult`, `SetEnChantResult`, `SetRestoreResult`, `ShowPetInputPopup`, `SocketItemResult`, `TransformCubeResult`, `TransformMetalResult`, `TransformProductionResult`, `TransformRelicResult`, `TransmuteFortuneCubeResult`, `UnEquipResult`, `UnbindingItemResult`, `UseItemResult`, `VehicleSpeedUpItemResult`, `WithdrawMoneyFromPrivateBankResult`
- **Client+Server** (3): `HideUI`, `ItemEffectFnCallResult`, `ShowUI`
- **Server** (32): `AddItemToPrivateBank`, `AttachCore`, `AttachRelic`, `BuyItemFromNPC`, `DepositMoneyToPrivateBank`, `Drop`, `DropMoney`, `Equip`, `ItemCreateSocket`, `ItemDisassemble`, `ItemFeatureAppraisal`, `MoveItemInContainer`, `NLogCashEquip`, `PetDetectMoneyServer`, `Pickup`, `PowerStoneDisassemble`, `RemoveItemFromPrivateBank`, `RepurchaseItemFromNPC`, `SelectEquipmentPageProcess`, `SellItemToNPC`, `SetEnChantServer`, `SetRestoreServer`, `TransformCube`, `TransformMetal`, `TransformMultiCube`, `TransformProduction`, `TransformRelic`, `TransmuteFortuneCube`, `UnEquip`, `UseItem`, `UseVehicleUpItem`, `WithdrawMoneyFromPrivateBank`

### `Channel`  (66 RPCs) — `Class/Main/Channel.lua`
- **Master** (10): `BroadcastEventOnMaster`, `Master_ForwardToGIP`, `Master_ForwardToPaymentMgrProxy`, `Master_ForwardToTencentAntiHackProxy`, `Master_MarketOnReqGoodsList`, `Master_OnGetChannelUserLimit`, `Master_OnSetChannelUserLimit`, `Master_PostSendGift`, `Master_PostSendMailWithItem`, `WorldBossDeadMessage`
- **Server** (55): `AppointSpawnMob`, `BroadcastEventOnServer`, `CheckInsAliveFromMaster`, `ClearWorldBoss`, `DeleteAppointSpawnMob`, `OnServerPCRoomEnable`, `OnServer_UpdateSelectedRandomQuests`, `SendSlidingMsgChannel`, `Server_ForwardToPaymentMgr`, `Server_MarketOnResCancel`, `Server_MarketOnResGoodsList`, `Server_MarketOnResSalesGoodsList`, `Server_OnCheckClientFile`, `Server_OnGetChannelUserLimitFromMaster`, `Server_OnResetAntiHackConfig`, `Server_OnResetCDGConfig`, `Server_OnSendGift`, `Server_OnSendMailWithItem`, `Server_OnSetAntiFishingMessage`, `Server_OnSetAntiHack`, `Server_OnSetAntiHackEnable`, `Server_OnSetAntiHackSub`, `Server_OnSetBlockBingo`, `Server_OnSetBlockCommonSkill`, `Server_OnSetBlockCube`, `Server_OnSetBlockGoddessSkill`, `Server_OnSetBlockInteractNpc`, `Server_OnSetBlockPortal`, `Server_OnSetCashShopEnable`, `Server_OnSetCheckClientFile`, `Server_OnSetCheckInject`, `Server_OnSetDailyResetTime`, `Server_OnSetExchangerEnable`, `Server_OnSetExpRateHunt`, `Server_OnSetFGManager`, `Server_OnSetFGManagerEnable`, `Server_OnSetInvalidFileKick`, `Server_OnSetItemDropRate`, `Server_OnSetMailBoxEnable`, `Server_OnSetMaxLevel`, `Server_OnSetPCRoomEnable`, `Server_OnSetResAntiHack`, `Server_OnSetVIPEnable`, `Server_OnSetVulfarismFilterEnable`, `Server_OnUpdateVulgarismFilterData`, `SlidingMessageEvent`, `SpawnWorldBoss`, `SpawnWorldBossMessage`, `UpdateCapsuleNotifyList_Server`, `onLoadNoticeMessage`, `onLoadWeeklyBingo`, `onPartyCreated`, `onPartyDeleted`, `onRunCommandDeleteNoticeMessage`, `onRunCommandNoticeMessage`
- **Server+Master** (1): `AliveInsResult`

### `QuestManager`  (60 RPCs) — `Class/Main/Quest/QuestManager.lua`
- **?** (1): `SetQuestMonsterList`
- **Client** (31): `BingoSetTableUI`, `BingoStateNotify`, `InsMissionEndClient`, `InsMissionStartClient`, `LuckyCellProc`, `MissionCompleteResult`, `NotifyAutoQuestComplete`, `NotifyClientQuestAdd`, `NotifyClientQuestFinish`, `Notify_BingoComplete`, `OnClientSetQuestAlarmResult`, `OnNotifyUpdateMissionQuest`, `OnSetFirstMissionQuest`, `OnUpdateMissionStepList`, `OnUpdateStepList`, `OpenBingoBoard`, `ProcessWeeklyBgEventResult`, `Result_SendBingReward`, `RewardProc_CRCEResult`, `RewardProc_CenterResult`, `SendWeeklyDataClient`, `SetQuestRequirementValue`, `SetSyncTimeLimitUI`, `ShowBingoMissionPopup`, `UpdateCancelMission`, `UpdateWeeklyBingoDataResult`, `autoSignShow`, `autoSignUpdate`, `completeAutoSignUpdate`, `completeautoSignShow`, `onTest`
- **Client+Server** (5): `DeltaQuestTimestamp`, `ResetDailyLimitTime`, `SetEpNextQuest`, `SetNewQuestAlarm`, `addMissionBonus`
- **Master** (1): `MasterOnPushCheck`
- **Server** (22): `AllClearBingoMission`, `BingoCompleteCenter`, `CanStartBingo`, `CancelMission`, `CreateBingoBoard`, `CreateBingoCell`, `ForceClearMission`, `ForceResetWeeklyBingoData`, `ForceWeeklyBingoMission`, `InitBingoData`, `OnChangeMissionQuest`, `OnMissionStepAlarm`, `OnSelectMissionQuest`, `OnServerSetQuestAlarm`, `RequestTimeLimitSync`, `ResetEpisodeQuest`, `RewardProc_CRCE`, `ServerOnPushCheck`, `SetSpecialInsMission`, `addMissionBonusFromClient`, `enableFastQuestHack`, `finishRepeatCollectionQuest`

### `GuildProxy`  (55 RPCs) — `Class/Main/Community/GuildProxy.lua`
- **?** (1): `onUpdateGuildState`
- **Client** (24): `OnClientBroadcast`, `OnGuildExpand`, `OnGuildLevelUp`, `UpdateGuildExp`, `onAutoPromote`, `onDissolveGuild`, `onGuildChat`, `onInviteAcceptFail`, `onInviteRequestAccept`, `onInviteRequestDeny`, `onInviteResult`, `onKickMember`, `onLeaveGuild`, `onLeaveMember`, `onMapChangeResult`, `onMemberConnect`, `onMemberDisconnect`, `onNoticeChanged`, `onPromote`, `onPromoteFail`, `onSetGuild`, `onSetGuildForReplicate`, `onUpdateGrade`, `onUpdateMemeberInfo`
- **Client+Server** (4): `onAddMember`, `onAuthorityChange`, `onCallsignChange`, `onCheckInviteRequest`
- **Client+Server+Master** (1): `ClearGuildData`
- **Master** (8): `EstablishGuildForce`, `onChangeCallSign`, `onChangeGradeAuthority`, `onChangeNotice`, `onEstablishGuild_Master`, `onJoin`, `onRequestDissolution`, `sendChat`
- **Server** (5): `RunScript`, `UpdateMaster`, `onEstablishGuild_Server`, `onEstablishGuild_ServerResult`, `onSetGuild_Server`
- **Server+Master** (12): `ForceDissolve`, `GuildNameCheck`, `UpdateMemberInfo`, `connect`, `inviteRequestAccept`, `inviteRequestDeny`, `onChangeGrade`, `onInvite`, `onKick`, `onLeave`, `onMapChange`, `promote`

### `ActorController`  (53 RPCs) — `Class/Main/ActorController.lua`
- **?** (1): `SetVariable`
- **Client** (22): `CollectionResult`, `DownCallForceStartSkill`, `DownCallPlaySound`, `DownCallPlayerTalk`, `FloatUp`, `LuaNetFireProjectile`, `NetActionWeakFreeze`, `NetChangeBosslistener`, `NetChangePlayerlistener`, `NetLuaBuffCombatEffect`, `NotifyCannotCollect`, `OnClientGetCellInfoResult`, `OnLevelUp`, `OnPlayDeleteEffect`, `OnPlayEffect`, `OnReplicateCurrentState`, `OnRequestUserInfo`, `OnSkillPointUp`, `OnTotalDamageResult`, `OutD3DRenderUse`, `SetLimitedSpecial`, `ThrowItemResult`
- **Client+Server** (10): `ApplyModifier`, `ChangePositionTarget`, `ForceUpdateModifier`, `NetRestoreTransformedBody`, `NetTransformBody`, `OnQuitFloatUp`, `PlayerCancelAction`, `SetStoryManagerVariable`, `UpdateActBIT`, `UpdateLevelTagByFavor`
- **Server** (19): `ClassUpWrapper`, `CreateMpRegenHelper`, `DecreaseHyperGauge`, `ForceApplyOrb`, `ForceUpdateModifierVariable`, `GetCollectionItem`, `GetMiningItem`, `IncreaseStatSpecific`, `NetLuaBuffCombat`, `OnServerGetCellInfo`, `QuitFloatUpServer`, `ReplicateCurrentState`, `ReplyUserInfo`, `RequestSummonMonsterAtPosition`, `ResetCollectionTarget`, `SetBuffVariable`, `SetPositionTarget`, `SkillBookLevelUp`, `TotalDamageCheckEvent`
- **Server+Client** (1): `LuaCallSkillFunction`

### `PlayerController`  (34 RPCs) — `Class/Main/PlayerController.lua`
- **Client** (13): `ClientOnDeadForParty`, `NetForceDeleteProjectileAll`, `OnCombatCancelEvent`, `OnCombatEndEvent`, `OnCombatStartEvent`, `ProcessPlayerRespawn_Result`, `ProcessStaminaResult`, `RequestCombatMessage`, `SendCombatMessageOnLooker`, `SendDescExpToClient`, `SoulOrbProcessClient`, `Update_UIBodyTagPlayerColor`, `WaitingCombatMessage`
- **Client+Server** (4): `NetDeleteSavedStatus`, `NetGuardFunction`, `NetSaveCurrentStatus`, `SetCombatTypeToClientAndServer`
- **Server** (17): `ApplyElementalModifier`, `AttackAuraServerProcess`, `CheckPvPObjectDestroy`, `DeleteAura`, `MagicalDefenseAuraServerProcess`, `NetAlchemySpellProcess`, `OnCancelCombat`, `OnConfirmCombat`, `PhysicalDefenseAuraServerProcess`, `ProcessPlayerRespawn`, `ProcessPlayerReturn`, `RequestCombat`, `RequestDeadForParty`, `RequestMoveToMap`, `RequestQuestObjDeadProcess`, `SoulOrbProcessServer`, `VitalAuraServerProcess`

### `CCommunity`  (34 RPCs) — `Class/Main/Community/CCommunity.lua`
- **Client** (18): `AddBlokListResult`, `OnClient_AddBuddy`, `OnClient_AddFriend`, `OnClient_AddRpm`, `OnClient_DeleteBuddy`, `OnClient_DeleteFriend`, `OnClient_DeleteRpm`, `OnClient_InitAddBuddy`, `OnClient_InitAddFriend`, `OnClient_InitAddRpm`, `OnClient_LogIn`, `OnClient_PlayerLoggedIn`, `OnClient_RequestBuddy`, `OnClient_SetPlayerJournal`, `OnClient_StatusChange`, `OnClient_UpdateBuddyRelationPosition`, `OnClient_UpdateBuddyRelationType`, `RemoveBlockListResult`
- **Client+Master** (2): `AddBlockList`, `RemoveBlockList`
- **Master** (12): `Master_AddBuddy`, `Master_AddFriend`, `Master_AddFriendByName`, `Master_DeleteBuddy`, `Master_DeleteFriend`, `Master_GetPlayerJournal`, `Master_RejectRequestBuddy`, `Master_SendRequestBuddy`, `Master_SendRequestBuddyByName`, `Master_StateBuddyDeny`, `OnMaster_LogIn`, `RequestMaster_UpdateState`
- **Server** (2): `OnServer_CommunityNotifyOverCnt`, `OnServer_UpdateBuddyRelationPosition`

### `AIController`  (33 RPCs) — `Class/Main/AI/AIController.lua`
- **Client** (7): `CallProcessEvent`, `ClientOnDamaged`, `DispatchEvent`, `DownCallDispatchEvent`, `FireAbsorbProjectile`, `OnClientStartEventSkill`, `onClientTalkAround`
- **Client+Server** (3): `KillMonsterOnDeadSkillAbility`, `ProcessMobDelete`, `ServerOnMapEvent`
- **Client+Server+Master** (1): `NotifyPosition`
- **Server** (22): `ActionDropItem`, `ChangePosByJakil`, `ClearRoomMonster`, `ClearWorldBoss`, `DefenseFail`, `FailTimeLimitIns`, `HidingExCommand`, `IncreaseHitCount`, `KickInsPlayers`, `ModifyStat`, `ModifyStatList`, `NetSetMonsterCollisionHeight`, `OnServerForceKillMonster`, `ProcessModDeadBridge`, `RecoveryHPPercent`, `RollbackModifyStatList`, `RoomPlayerCameraShake`, `SkillStartNotify`, `SummonMonster`, `SummonMonsterAnger`, `SummonMonsterArea`, `onServerTalkAround`

### `Exchanger`  (29 RPCs) — `Class/Main/Item/Exchanger.lua`
- **Client** (14): `HideDialog`, `NetLogError`, `OnCancel`, `OnClientActionRequest`, `OnClientAuctionAccept`, `OnClientAuctionDeny`, `OnFail`, `OnFailOther`, `OnRequest`, `OnSuccess`, `OnTraderClientAuctionAccept`, `OnTraderClientAuctionCancel`, `OnTraderClientAuctionRequest`, `RequestResult`
- **Server** (15): `AddItem`, `AddItemForBossCard`, `AddMoney`, `AuctionAccept`, `AuctionCancel`, `AuctionDeny`, `AuctionRequest`, `Cancel`, `Ok`, `OnRequestResult`, `OnSuccessAuction`, `RemoveItem`, `RemoveMoney`, `Request`, `Trade`

### `User`  (23 RPCs) — `Class/Main/User.lua`
- **?** (2): `GetQuestCount`, `SpecialInsMove`
- **Client** (11): `AfterForcedMovePlayer`, `ClientNotifyOffline`, `ClientNotifyOnline`, `ClientOnMapChangedForParty`, `DirectSceneDetach`, `DownSetEndCutScene`, `Map_LoadedEvent`, `OnBeforeMapLoadingProcess`, `SetDefaultCamera`, `SetRememberT`, `TempVolumeZero`
- **Client+Server** (1): `PutItemExResult`
- **Server** (8): `ChangeRoom`, `DirectScenePlayerDetach`, `LeaveInstance`, `MoveToInstanceMap`, `MoveToMapWrapper`, `MoveToTown`, `RequestDeadForParty`, `RequestMapChangedForParty`
- **Server+Master** (1): `SetSaveRoomName`

### `ZoneBossInstanceManager`  (22 RPCs) — `Class/Main/ZoneBossInstanceManager.lua`
- **Client** (17): `AttackCountUIUpdate`, `AttackCountUIUpdate_InsMission`, `HideAttackCountUI`, `HideCardSelectionDialog`, `HideTimeLimitUI`, `NotMoveToMapPositionMessage`, `OnCloseTimeDialog`, `OnShowDialogMessage`, `OnShowRankResultDialogMessage`, `PickItemResult`, `SelectCardResult`, `ShowAttackCountUI`, `ShowAttackCountUI_InsMission`, `ShowCardReulst`, `ShowCardSelectionDialog`, `ShowTimeLimitUI`, `ShowTimeLimitUI_InsMission`
- **Server** (5): `AttackCountUICheck`, `AttackCountUICheck_InsMission`, `SelectCard`, `TimeLimitUICheck`, `TimeLimitUICheck_InsMission`

### `CChat`  (20 RPCs) — `Class/Main/CChat.lua`
- **Client** (11): `AddNoticeMessage`, `DeleteNoticeMessage`, `MTMReceiveMessage`, `OnCheckPlayerResult`, `OnClientBroadcastProcess`, `OnRequestExecuteCommand`, `OnSendingMessageUpdate`, `ShowClientSlidingMsg`, `ShowSlidingMessageEvent`, `ShowWorldBossMessageEvent`, `onBroadcastEvent`
- **Client+Server+Master** (1): `SetNoTalkEndTime`
- **Master** (2): `OnMasterBroadcastProcess`, `SendAllPlayersSlidingMsg`
- **Server** (6): `CheckPlayer`, `DeletePlayer`, `EmotionEvent`, `MTMSendMessage`, `PreBroadcast`, `RequestExecuteCommand`

### `Party`  (20 RPCs) — `Class/Main/Party/Party.lua`
- **Client** (1): `OnSetMessage`
- **Client+Server** (3): `onAddOfflineMember`, `onDeleteOfflineMember`, `onPromote`
- **Master** (1): `Master_setChannelId`
- **Server** (7): `Server_setChannelId`, `SetPickupRuleUpdate`, `onAdd`, `onChannelAdded`, `onMemberChange`, `onRemove`, `onSummon`
- **Server+Master** (8): `ClearGuildData`, `DenyRequestParty`, `OnSetPartyInfo`, `RequestGuildConfirm`, `connect`, `onDisconnect`, `onRequestGuildConfirm`, `setGuildEstablishing`

### `PersonalStore`  (18 RPCs) — `Class/Main/Item/PersonalStore.lua`
- **Client** (11): `CloseStoreResult`, `CloseStoreTagUpdate`, `ItemBuyResult`, `ItemSellResult`, `ModifyStoreResult`, `ModifyStoreTagUpdate`, `ResultDetachItemInStore`, `StartStoreResult`, `UpdateItemListForStackable`, `onClientEnterStore`, `onClientLeaveStore`
- **Server** (7): `CloseStore`, `DetachItemInStore`, `EnterStore`, `LeaveStore`, `ModifyStore`, `RequestBuyItem`, `StartStore`

### `CMarket`  (17 RPCs) — `Class/Main/CMarket.lua`
- **Client** (9): `OnNotifyEnabled`, `OnResBuyClient`, `OnResCancelClient`, `OnResEventClient`, `OnResGoodsListClient`, `OnResLatestPriceClient`, `OnResRegisterGoodsClient2`, `OnResSalesGoodsCountClient`, `OnResSalesGoodsListClient`
- **Master** (2): `OnReqCancelMaster`, `OnReqGoodsListMaster`
- **Server** (6): `OnReqBuyServer`, `OnReqRegisterGoodsBunchServer`, `OnReqRegisterGoodsServer`, `OnReqSalesGoodsCountServer`, `OnResBuyServer`, `OnResRegisterGoodsServer`

### `CStatus`  (16 RPCs) — `Class/Main/CStatus.lua`
- **Client** (2): `OnClientUpdateSkillTreeDialog`, `ResultGetCurrentBuffs`
- **Client+Server** (5): `AddBuff`, `AddBuffInfo`, `ChangeBuffData`, `DelBuff`, `DoModifications`
- **Client+Server+Master** (4): `AddBuff2`, `PrintBuff2`, `RemoveBuff2`, `UpdateBuff2`
- **Server** (5): `OnAddValue`, `OnApply_TotemBuff`, `OnServerGetCurrentBuffs`, `PoisonDamage`, `UnitTest`

### `PlayerController_Fishing`  (15 RPCs) — `Class/Main/PlayerController_Fishing.lua`
- **Client** (10): `FishingResult`, `HideFishingWaitGaugeUI`, `OnSetFishingArea`, `SetActorDirectionForFishing`, `SetForceFishingAreaUnTouch`, `ShowFishingBarUI`, `ShowFishingRewardItemUI`, `ShowFishingWaitGaugeUI`, `ShowMessageFishingFail`, `ShowMessageFishingToolWornout`
- **Server** (5): `RequestCancelFishing`, `RequestEndFishingFish`, `RequestEndFishingFishEmotion`, `RequestFishingFish`, `ServerSetFishingArea`

### `CMailBoxEx`  (15 RPCs) — `Class/Main/Community/CMailBoxEx.lua`
- **Client** (5): `onDeleteMailClient`, `onFetchMail`, `onGetGiftList`, `onReturnMailResult`, `onSendMailResult`
- **Master** (3): `findRcpt`, `onDeleteGiftMaster`, `onDeleteMailMaster`
- **Server** (4): `DeleteGift`, `deleteMail`, `returnMail`, `sendMail`
- **Server+Master** (3): `fetchMail`, `getGiftList`, `postMail`

### `CStatusPlayer`  (14 RPCs) — `Class/Main/CStatusPlayer.lua`
- **Client** (9): `AddSkillHotKey`, `ClassUpResult`, `IncreasePlayerExpResult`, `LevelUpResult`, `MessageShow`, `NotifySetPlayerLevel`, `OnSaveRandomBuffValue`, `RepairAllResult`, `RepairEquipResult`
- **Server** (5): `AddModifierStat`, `NetMakeRealMinEquipment`, `RepairAllRequest`, `RepairEquipRequest`, `StatUp`

### `CEquipment`  (13 RPCs) — `Class/Main/Item/CEquipment.lua`
- **Client** (8): `DisableAttributeChanged`, `NetHairColorInfo`, `NetHairInfo`, `NetOnUpdateClient`, `ReloadDescription`, `SetOnPowerStoneResult`, `SetPowerStoneSlot`, `UpdateRepairHUD`
- **Server** (4): `DecreaseDurability`, `RedoItemModification`, `SetCasualOptions`, `SetOnPowerStone`
- **Server+Master** (1): `LuaDoModification`

### `NpcControllerEx`  (11 RPCs) — `Class/Main/NpcControllerEx.lua`
- **Client** (4): `OnDialogQuitClient`, `OnDialogRequestClient`, `OnStateChangedClient`, `onResultNpcMoveToMap`
- **Server** (7): `OnDialogAcceptServer`, `OnDialogQuitServer`, `OnDialogRequestServer`, `onNpcMoveToMap`, `onSaveComebackPosition`, `onSaveNpcPathData`, `onSaveNpcWayPointIndex`

### `CSkillBook`  (10 RPCs) — `Class/Main/Skill/CSkillBook.lua`
- **?** (1): `SetAttributeLevel_Master`
- **Client** (3): `OnClearSkillPoint`, `OnSkillLevelDown`, `OnSkillLevelUp`
- **Client+Server** (1): `SkillBuy`
- **Client+Server+Master** (1): `NotifySetEnable`
- **Server** (4): `AttributeLevelUpEx`, `ClearSkillPoint`, `SkillLevelUp`, `SkillLevelUpEx`

### `CInventory`  (8 RPCs) — `Class/Main/Item/CInventory.lua`
- **Client** (4): `OnCheckAlchemyResult`, `OnClientSetUseItemInfo`, `SetRepurchaseItem`, `onItemSkillResult`
- **Server** (4): `CheckAlchemyResult`, `DecreaseDurabilityItem`, `GetRepairCostItem`, `ItemSkillResult`

### `CPrivateStatus_HotKeySettings`  (7 RPCs) — `Class/Main/CPrivateStatus_HotKeySettings.lua`
- **Client** (2): `OnChangeQuickKeySet`, `SetHKeyCnf`
- **Server** (5): `ChangeQuickKeySet`, `DelHotKey`, `DelQuickKey`, `SaveHotKey`, `SaveQuickKey`

### `Trigger`  (7 RPCs) — `Class/Main/Trigger.lua`
- **Client** (1): `SetMTEnabled`
- **Client+Server** (1): `MoveToMapPosition`
- **Server** (5): `LuaOnServerTouch`, `LuaOnServerUnTouch`, `MoveToInstanceMap`, `ProcessTownPortalReturn`, `ProcessTownPortalToTown`

### `CashProductList`  (7 RPCs) — `Class/Main/CashProduct/CashProductList.lua`
- **Client** (3): `ProductReloaded`, `RequestProductListResult`, `RequestProductsByCodeResult`
- **Master** (1): `RequestProductListToMaster`
- **Server** (3): `OnRequestProductList`, `OnRequestProductsByCode`, `ReplyProductList`

### `CGuild`  (7 RPCs) — `Class/Main/Community/CGuild.lua`
- **Master** (2): `AddExp_Master`, `ExpandMember`
- **Server** (3): `OnUpdateGuildExp`, `Wraper_LogGuildKick_Server`, `Wraper_LogGuildLevelUp_Server`
- **Server+Master** (2): `OnLevelUp`, `connect`

### `CStatusItem`  (6 RPCs) — `Class/Main/Item/CStatusItem.lua`
- **?** (2): `RepairResult`, `c_SetBinded`
- **Client** (2): `ItemExpired_Client`, `NetCallSetFunc`
- **Master** (1): `onSaveRelation`
- **Server** (1): `RepairRequest`

### `TriggerCommands`  (5 RPCs) — `Class/Main/TriggerCommands.lua`
- **Client** (2): `AttachPartyPlayer`, `ServerLuaTouchCheckResult`
- **Server** (3): `ServerLuaTouchCheck`, `UpdateTouched`, `UpdateTouchedPlayerOID`

### `AIController_Ability`  (5 RPCs) — `Class/Main/AI/AIController_Ability.lua`
- **Server** (2): `NetApplyAbilityImmuneAttackBuff`, `NetDispatchAbilityEvent`
- **Server+Master** (3): `NotifyAbilityExecute`, `SetAbilityParameter`, `SetAbilityParameterList`

### `CItemContainer`  (5 RPCs) — `Class/Main/Item/CItemContainer.lua`
- **Client** (2): `NetSetSlotInfo`, `Popup_AddTabMax`
- **Client+Server** (2): `AddTab`, `BankSort`
- **Server** (1): `SetSelectTab`

### `PetController`  (5 RPCs) — `Class/Main/Pet/PetController.lua`
- **?** (5): `TalkToPlayer`, `onPut`, `onTeleport`, `remove`, `teleport`

### `PetStatus`  (5 RPCs) — `Class/Main/Pet/PetStatus.lua`
- **Client** (5): `AddCoreInfoClient`, `ChangePowerValueResult`, `IncreaseRelationShipResult`, `LevelUpResult`, `RenamePetClient`

### `Session`  (4 RPCs) — `Class/Main/Session.lua`
- **?** (1): `RequestGPS`
- **Client** (2): `GPSDataClient`, `onRequestGPS`
- **Server** (1): `GPSDataServer`

### `Component`  (4 RPCs) — `Lib/Component.lua`
- **Client+Server+Master** (4): `BroadcastCallRelay`, `OnReplicateCallClient`, `SetVariable`, `SetVariables`

### `CBody`  (3 RPCs) — `Class/Main/CBody.lua`
- **Client+Server** (1): `UnDressGoddessEquip`
- **Server** (2): `NotifyScaleToServer`, `SetUnlimitedAction`

### `CBombController`  (3 RPCs) — `Class/Main/CBombController.lua`
- **Client** (2): `Boom`, `OnSetEnabled`
- **Server** (1): `AddDamageMonsters`

### `Interacter`  (3 RPCs) — `Class/Main/Interacter.lua`
- **Client** (1): `OnChangeQuest`
- **Server** (2): `InteractServer`, `OnInteract`

### `CJournal`  (3 RPCs) — `Class/Main/Community/CJournal.lua`
- **?** (2): `OnGetJournalData`, `Server_GetJournalData`
- **Master** (1): `OnMaster_SetJournalStatus`

### `DynamicObjectController`  (2 RPCs) — `Class/Main/DynamicObjectController.lua`
- **?** (1): `SetDead`
- **Server** (1): `ProcessDeadFirst`

### `NpcInteracter`  (2 RPCs) — `Class/Main/NpcInteracter.lua`
- **Server** (2): `FocusIn`, `FocusOut`

### `CItemController`  (2 RPCs) — `Class/Main/Item/CItemController.lua`
- **Client** (2): `PickupMove`, `SetIgnore`

### `PartyList`  (2 RPCs) — `Class/Main/Party/PartyList.lua`
- **Client** (1): `RequestPartyListResult`
- **Server** (1): `RequestPartyList`

### `Quest`  (2 RPCs) — `Class/Main/Quest/Quest.lua`
- **?** (2): `MasterOnUpdateCheck`, `ServerOnUpdataCheck`

### `CAdminClient`  (2 RPCs) — `ClassEx/ClientOnly/CAdminClient.lua`
- **Client** (2): `OnRunAdminCommandAtClient`, `OnRunCommandResult`

### `CFieldController`  (1 RPCs) — `Class/Main/CFieldController.lua`
- **Client** (1): `OnSetEnabled`

### `CPrivateStatus_Fishing`  (1 RPCs) — `Class/Main/CPrivateStatus_Fishing.lua`
- **Client** (1): `IncreaseFishingExp_NotifyMessage`

### `CTrigger`  (1 RPCs) — `Class/Main/CTrigger.lua`
- **Client+Server** (1): `ProcessCommandResult`

### `Physics`  (1 RPCs) — `Class/Main/Physics.lua`
- **Client** (1): `OnClearMotion`

### `CAccountBank`  (1 RPCs) — `Class/Main/Item/CAccountBank.lua`
- **Client+Server** (1): `BankSort`

### `Controller`  (1 RPCs) — `Lib/Com/Controller.lua`
- **Client+Server** (1): `ForceRollbackModifier`

### `TSkill_Magician_ChainLightning`  (1 RPCs) — `Object/Main/Skill/Magician/TSkill_Magician_ChainLightning.lua`
- **?** (1): `OnChangeTargets`

### `TSkill_Magician_Lightning`  (1 RPCs) — `Object/Main/Skill/Magician/TSkill_Magician_Lightning.lua`
- **?** (1): `OnChangeTarget`

### `TSkill_Magician_LightningCurse`  (1 RPCs) — `Object/Main/Skill/Magician/TSkill_Magician_LightningCurse.lua`
- **?** (1): `OnChangeTarget`

### `TSkill_Magician_LightningSpark`  (1 RPCs) — `Object/Main/Skill/Magician/TSkill_Magician_LightningSpark.lua`
- **?** (1): `OnChangeTarget`

### `TSkill_Magician_LightningThunderFog`  (1 RPCs) — `Object/Main/Skill/Magician/TSkill_Magician_LightningThunderFog.lua`
- **?** (1): `OnChangeTarget`

### `TSkill_Magician_MovingLightning`  (1 RPCs) — `Object/Main/Skill/Magician/TSkill_Magician_MovingLightning.lua`
- **?** (1): `OnChangeTargets`

### `TSkill_Magician_ContinuousLightning`  (1 RPCs) — `Object/Main/Skill/Magician/LightningMagician/TSkill_Magician_ContinuousLightning.lua`
- **Server** (1): `OnChangeTargets`

### `TSkill_Monster_Lightning`  (1 RPCs) — `Object/Main/Skill/Monster/TSkill_Monster_Lightning.lua`
- **?** (1): `OnChangeTargets`

### `TSkill_Monster_LightningCurse_All`  (1 RPCs) — `Object/Main/Skill/Monster/TSkill_Monster_LightningCurse_All.lua`
- **?** (1): `OnChangeTarget`
