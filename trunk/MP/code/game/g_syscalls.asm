code

equ	trap_Print				-1
equ	trap_Error				-2
equ	trap_Milliseconds		-3
equ	trap_Cvar_Register		-4
equ	trap_Cvar_Update		-5
equ	trap_Cvar_Set			-6
equ	trap_Cvar_VariableIntegerValue	-7
equ	trap_Cvar_VariableStringBuffer	-8
equ	trap_Argc				-9
equ	trap_Argv				-10
equ	trap_FS_FOpenFile		-11
equ	trap_FS_Read			-12
equ	trap_FS_Write			-13
equ trap_FS_Rename			-14
equ	trap_FS_FCloseFile		-15
equ	trap_SendConsoleCommand	-16
equ	trap_LocateGameData		-17
equ	trap_DropClient			-18
equ	trap_SendServerCommand	-19
equ	trap_SetConfigstring	-20
equ	trap_GetConfigstring	-21
equ	trap_GetUserinfo		-22
equ	trap_SetUserinfo		-23
equ	trap_GetServerinfo		-24
equ	trap_SetBrushModel		-25
equ	trap_Trace				-26
equ	trap_PointContents		-27
equ trap_InPVS				-28
equ	trap_InPVSIgnorePortals	-29
equ	trap_AdjustAreaPortalState	-30
equ	trap_AreasConnected		-31
equ	trap_LinkEntity			-32
equ	trap_UnlinkEntity		-33
equ	trap_EntitiesInBox		-34
equ	trap_EntityContact		-35
equ	trap_BotAllocateClient	-36
equ	trap_BotFreeClient		-37
equ	trap_GetUsercmd			-38
equ	trap_GetEntityToken		-39
equ	trap_FS_GetFileList		-40
equ trap_DebugPolygonCreate	-41
equ trap_DebugPolygonDelete	-42
equ trap_RealTime			-43
equ trap_SnapVector			-44
equ trap_TraceCapsule		-45
equ trap_EntityContactCapsule	-46
equ trap_GetTag				-47

equ	memset					-101
equ	memcpy					-102
equ	strncpy					-103
equ	sin						-104
equ	cos						-105
equ	atan2					-106
equ	sqrt					-107
equ floor					-111
equ	ceil					-112
equ	testPrintInt			-113
equ	testPrintFloat			-114



equ trap_BotLibSetup					-201
equ trap_BotLibShutdown					-202
equ trap_BotLibVarSet					-203
equ trap_BotLibVarGet					-204
equ trap_BotLibDefine					-205
equ trap_BotLibStartFrame				-206
equ trap_BotLibLoadMap					-207
equ trap_BotLibUpdateEntity				-208
equ trap_BotLibTest						-209

equ trap_BotGetSnapshotEntity			-210
equ trap_BotGetServerCommand		-211
equ trap_BotUserCommand					-212



; BOTLIB_AAS_ENTITY_VISIBLE				-301
; BOTLIB_AAS_IN_FIELD_OF_VISION			-302
; BOTLIB_AAS_VISIBLE_CLIENTS			-303
equ trap_AAS_EntityInfo					-304

equ trap_AAS_Initialized				-305
equ trap_AAS_PresenceTypeBoundingBox	-306
equ trap_AAS_Time						-307

equ trap_AAS_SetCurrentWorld			-308

equ trap_AAS_PointAreaNum				-309
equ trap_AAS_TraceAreas					-310

equ trap_AAS_PointContents				-311
equ trap_AAS_NextBSPEntity				-312
equ trap_AAS_ValueForBSPEpairKey		-313
equ trap_AAS_VectorForBSPEpairKey		-314
equ trap_AAS_FloatForBSPEpairKey		-315
equ trap_AAS_IntForBSPEpairKey			-316

equ trap_AAS_AreaReachability			-317

equ trap_AAS_AreaTravelTimeToGoalArea	-318

equ trap_AAS_Swimming					-319
equ trap_AAS_PredictClientMovement		-320

equ trap_AAS_RT_ShowRoute				-321
equ trap_AAS_RT_GetHidePos				-322
equ trap_AAS_FindAttackSpotWithinRange	-323
equ trap_AAS_SetAASBlockingEntity		-324


equ trap_EA_Say							-401
equ trap_EA_SayTeam						-402
equ trap_EA_UseItem						-403
equ trap_EA_DropItem					-404				
equ trap_EA_UseInv						-405
equ trap_EA_DropInv						-406
equ trap_EA_Gesture						-407
equ trap_EA_Command						-408

equ trap_EA_SelectWeapon				-409
equ trap_EA_Talk						-410
equ trap_EA_Attack						-411
equ trap_EA_Reload						-412
equ trap_EA_Use							-413
equ trap_EA_Respawn						-414
equ trap_EA_Jump						-415
equ trap_EA_DelayedJump					-416
equ trap_EA_Crouch						-417
equ trap_EA_MoveUp						-418
equ trap_EA_MoveDown					-419
equ trap_EA_MoveForward					-420
equ trap_EA_MoveBack					-421
equ trap_EA_MoveLeft					-422
equ trap_EA_MoveRight					-423
equ trap_EA_Move						-424
equ trap_EA_View						-425

equ trap_EA_EndRegular					-426
equ trap_EA_GetInput					-427
equ trap_EA_ResetInput					-428



equ trap_BotLoadCharacter				-501
equ trap_BotFreeCharacter				-502
equ trap_Characteristic_Float			-503
equ trap_Characteristic_BFloat			-504
equ trap_Characteristic_Integer			-505
equ trap_Characteristic_BInteger		-506
equ trap_Characteristic_String			-507

equ trap_BotAllocChatState				-508
equ trap_BotFreeChatState				-509
equ trap_BotQueueConsoleMessage			-510
equ trap_BotRemoveConsoleMessage		-511
equ trap_BotNextConsoleMessage			-512
equ trap_BotNumConsoleMessages			-513
equ trap_BotInitialChat					-514
equ trap_BotReplyChat					-515
equ trap_BotChatLength					-516
equ trap_BotEnterChat					-517
equ trap_StringContains					-518
equ trap_BotFindMatch					-519
equ trap_BotMatchVariable				-520
equ trap_UnifyWhiteSpaces				-521
equ trap_BotReplaceSynonyms				-522
equ trap_BotLoadChatFile				-523
equ trap_BotSetChatGender				-524
equ trap_BotSetChatName					-525

equ trap_BotResetGoalState				-526
equ trap_BotResetAvoidGoals				-527
equ trap_BotPushGoal					-528
equ trap_BotPopGoal						-529
equ trap_BotEmptyGoalStack				-530
equ trap_BotDumpAvoidGoals				-531
equ trap_BotDumpGoalStack				-532
equ trap_BotGoalName					-533
equ trap_BotGetTopGoal					-534
equ trap_BotGetSecondGoal				-535
equ trap_BotChooseLTGItem				-536
equ trap_BotChooseNBGItem				-537
equ trap_BotTouchingGoal				-538
equ trap_BotItemGoalInVisButNotVisible	-539
equ trap_BotGetLevelItemGoal			-540
equ trap_BotAvoidGoalTime				-541
equ trap_BotInitLevelItems				-542
equ trap_BotUpdateEntityItems			-543
equ trap_BotLoadItemWeights				-544
equ trap_BotFreeItemWeights				-546
equ trap_BotSaveGoalFuzzyLogic			-546
equ trap_BotAllocGoalState				-547
equ trap_BotFreeGoalState				-548

equ trap_BotResetMoveState				-549
equ trap_BotMoveToGoal					-550
equ trap_BotMoveInDirection				-551
equ trap_BotResetAvoidReach				-552
equ trap_BotResetLastAvoidReach			-553
equ trap_BotReachabilityArea			-554
equ trap_BotMovementViewTarget			-555
equ trap_BotAllocMoveState				-556
equ trap_BotFreeMoveState				-557
equ trap_BotInitMoveState				-558
equ trap_BotInitAvoidReach				-559

equ trap_BotChooseBestFightWeapon		-560
equ trap_BotGetWeaponInfo				-561
equ trap_BotLoadWeaponWeights			-562
equ trap_BotAllocWeaponState			-563
equ trap_BotFreeWeaponState				-564
equ trap_BotResetWeaponState			-565
equ trap_GeneticParentsAndChildSelection -566
equ trap_BotInterbreedGoalFuzzyLogic	-567
equ trap_BotMutateGoalFuzzyLogic		-568
equ trap_BotGetNextCampSpotGoal			-569
equ trap_BotGetMapLocationGoal			-570
equ trap_BotNumInitialChats				-571
equ trap_BotGetChatMessage				-572
equ trap_BotRemoveFromAvoidGoals		-573
equ trap_BotPredictVisiblePosition		-574
equ trap_BotSetAvoidGoalTime			-575
equ trap_BotAddAvoidSpot				-576
equ trap_AAS_AlternativeRouteGoals		-577
equ trap_AAS_PredictRoute				-578
equ trap_AAS_PointReachabilityAreaIndex	-579

equ trap_BotLibLoadSource				-580
equ trap_BotLibFreeSource				-581
equ trap_BotLibReadToken				-582
equ trap_BotLibSourceFileAndLine		-583
 
