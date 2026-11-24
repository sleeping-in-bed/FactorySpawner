# Factory Spawner 命令手册与物品对照

适用版本为 Factory Spawner 2.9.2 和《幸福工厂》1.2.6（构建 520140）。把命令粘贴到游戏聊天框执行。各参数组用英文逗号分隔。

- [按产量生成生产线](#按产量生成生产线)
- [按台数生成机器排](#按台数生成机器排)
- [燃煤电厂示例](#燃煤电厂示例)
- [物品对照表](#物品对照表)

## 按产量生成生产线

`plan` 预览机器数量、外部输入、输出和预计峰值用电。`build` 使用同样的参数生成蓝图。右侧数值是每分钟的目标产量，可以用正小数。固体用件/分钟，液体和气体用 m³/分钟。规划器从当前存档已解锁的配方里选择机器和上游生产，矿石等原始资源默认由外部提供。

~~~
/FactorySpawner plan Rotor=10
/FactorySpawner build Rotor=10
/FactorySpawner build Rotor=10, Motor=5
~~~

在下方对照表搜索中文名，复制「物品 ID」一列填在等号左边。简体中文游戏内也可以输入 `转子=10`。多语言或重名时优先用物品 ID。物品完整类名 `Desc_Rotor_C` 也能用于 `plan` 和 `build`。

| 参数 | 作用 |
| --- | --- |
| `Rotor=10` | 让生产线每分钟输出 10 个转子。多个产物用逗号分隔。 |
| `import IronScrew` | 从外部输入螺丝，省去生产线内的螺丝加工。 |
| `recipe Rotor=Alternate_Rotor` | 指定转子使用钢制转子配方。左侧填物品 ID，右侧填配方 ID，该配方须已解锁。 |
| `export HeavyOilResidue` | 把重油残渣作为独立副产物输出，避免它参与这条产线的副产物回用。未被消耗的副产物也会自动安排出口。 |
| `floor Rotor=2` | 把配方 ID 为 `Rotor` 的机器固定到第 2 层。可指定第 1 至 8 层。`floor` 按配方定位，选用替代配方时应填写对应配方 ID。 |
| `floorHeight 32` | 指定相邻楼层的最小间距，范围 16–64 米。实际层距可以因设备高度和净空要求增大。 |
| `beltTier 3` | 使用 Mk.3 传送带和升降机。取值 1–6，省略时使用已解锁的最高等级。管道等级独立自动选择。 |
| `compact` | 缩短同排间距和固体产线的楼层净空。无地基时以 0.25 米为单位排楼层，带地基时沿用整米层距。 |
| `minMaterials` | 接完物流后比较原布局和其他楼层布局的建造清单，选择总件数最少的候选。 |
| `minMaterials IronPlate` | 先比铁板用量，再比总件数和占地。可换成其他物品 ID 或当前游戏语言的物品名。 |
| `vertical` | 尝试多个垂直布局，按接好物流之后的实际尺寸比较占地。 |
| `maxHeight 1000` | 和 `vertical` 一起用，给蓝图总高度设定上限，单位为整数米。`vertical` 的默认上限是 120 米。 |
| `noFoundations` | 省略蓝图的地基件，高层平台需要在现场另行处理。 |
| `noSigns` | 省略接口标牌，保留输入、输出和电路。 |

~~~
/FactorySpawner build Rotor=10, recipe Rotor=Rotor, floor Rotor=2
/FactorySpawner build Plastic=20, export HeavyOilResidue
/FactorySpawner build IronPlateReinforced=100, compact, noFoundations, noSigns
/FactorySpawner build SpaceElevatorPart_12=10, vertical, maxHeight 1000, noFoundations
~~~

蓝图默认用产物名称和每分钟产量或机器台数命名，并加上短后缀区分配置。`plan` 会预览蓝图名称。也可以在两种命令末尾追加 `, name 自定义名称`，名称支持中文和空格，最长 80 个字符。再次生成同名蓝图会替换原蓝图，名称比较不区分大小写。

~~~
/FactorySpawner build IronPlate=60, IronRod=30, IronScrew=120, name 基础铁件工厂
/FactorySpawner 5 CoalGenerator floor=1, 5 CoalGenerator floor=2, name 双层煤电厂
~~~

放下蓝图后连接外部原料、水或其他流体及电网，并为成品和副产物接好出口。

省材料模式保留指定的产量、配方、带速和地基选项。`plan` 会试建候选并显示建造材料，但不会保存蓝图。`build` 只保存胜出布局。可搭配 `vertical, maxHeight 120` 限制完成后的高度。比较范围是实际试建成功的候选。

~~~
/FactorySpawner plan IronPlate=60, IronRod=30, IronScrew=120, beltTier 1, noFoundations, minMaterials IronPlate
/FactorySpawner build IronPlate=60, IronRod=30, IronScrew=120, beltTier 1, noFoundations, minMaterials IronPlate
~~~

## 按台数生成机器排

~~~
/FactorySpawner 3 Assembler Rotor 75%, 2 Constructor IronRod floor=1, beltTier 3
~~~

一组的格式为 `台数 机器类型 [配方] [时钟百分比] [floor=N]`，台数是正整数。用逗号添加机器排或整张蓝图的设置。已指定配方的制造设备可以填写大于 0 且小于 100 的时钟百分比，省略时使用默认设置。配方位置填写表里的配方 ID，例如 `IngotIron`。这个位置接受单个词元，使用配方 ID 可避免配方显示名里的空格。

| 设备 | 机器类型参数 |
| --- | --- |
| 冶炼站 | `Smelter` |
| 构筑站 | `Constructor` |
| 装配站 | `Assembler` |
| 铸造站 | `Foundry` |
| 制造站 | `Manufacturer` |
| 精炼站 | `Refinery`、`OilRefinery` |
| 混料站 | `Blender` |
| 灌装站 | `Packager` |
| 转化站 | `Converter` |
| 粒子加速站 | `ParticleAccelerator`、`Accelerator`、`HadronCollider` |
| 量子编码站 | `QuantumEncoder`、`Encoder` |
| 煤炭发电机 | `CoalGenerator` |
| 燃油发电机 | `FuelGenerator` |
| 核电站 | `NuclearReactor`、`NuclearPowerPlant` |

- `floor=N` 放在机器组末尾，范围 1–8，省略时放在第 1 层。`floorHeight 32` 是整张蓝图的层距参数，默认 32 米，允许 16–64 米。
- `beltTier 3` 和 `noFoundations` 也可作为独立参数组使用。
- 多层机器排会尝试连接可识别的固体物流。可匹配的出料口来自命令中更早的机器排，一个出口只连接一个入口。流体管在每排内部铺设，各排的外部供液分别接入。

## 燃煤电厂示例

~~~
/FactorySpawner 5 CoalGenerator, 5 CoalGenerator
/FactorySpawner 5 CoalGenerator floor=1, 5 CoalGenerator floor=2
~~~

第一条生成同一层的两排机组，第二条生成两层各 5 台。发电机使用机器类型参数，煤和水从蓝图外接入。每排在 100% 时钟下消耗 75 煤/分钟和 225 m³ 水/分钟，发电 375 MW。两层共 750 MW。抽水机和给上层供水的管道泵要在现场接入。

## 物品对照表

中文名、英文名和 ID 来自该版本安装目录的 `CommunityResources/Docs/zh-Hans.json` 和 `en-US.json`。配方列列出以该物品为主产物、并由规划器支持的机器执行的配方。标有「副产物」的配方可通过 `recipe 物品ID=配方ID` 显式选择。实际可用配方以当前存档为准。原料默认外部供给，矿物转化配方要用 `recipe` 显式选择。

表涵盖原版的固体、液体、气体，包括采集品、节日物品和装备。配方栏为「—」的物品可按来源游戏机制取得，规划需要其中的原料时可用 `import` 指定外部输入。采矿机、建筑和车辆使用各自的建造方式，蓝图里的机器类型见上表。表按物品 ID 排序，可直接搜索中文名或英文名。

| 中文名 | 英文名 | 物品 ID | 形态 | 配方 ID |
| --- | --- | --- | --- | --- |
| 外星DNA胶囊 | Alien DNA Capsule | `AlienDNACapsule` | 固体 | `AlienDNACapsule` |
| 外星能量矩阵 | Alien Power Matrix | `AlienPowerFuel` | 固体 | `AlienPowerFuel` |
| 外星生物蛋白质 | Alien Protein | `AlienProtein` | 固体 | `Protein_Crab`、`Protein_Hog`、`Protein_Spitter`、`Protein_Stinger` |
| 氧化铝溶液 | Alumina Solution | `AluminaSolution` | 液体 | `AluminaSolution`、`UnpackageAlumina`、`Alternate_SloppyAlumina` |
| 铝制外壳 | Aluminum Casing | `AluminumCasing` | 固体 | `AluminumCasing`、`Alternate_AlcladCasing` |
| 铝锭 | Aluminum Ingot | `AluminumIngot` | 固体 | `IngotAluminum`、`PureAluminumIngot` |
| 复合铝板 | Alclad Aluminum Sheet | `AluminumPlate` | 固体 | `AluminumSheet` |
| 散热器 | Heat Sink | `AluminumPlateReinforced` | 固体 | `HeatSink`、`Alternate_HeatSink_1` |
| 碎铝渣 | Aluminum Scrap | `AluminumScrap` | 固体 | `AluminumScrap`、`Alternate_ElectroAluminumScrap`、`Alternate_InstantScrap` |
| 电池 | Battery | `Battery` | 固体 | `Battery`、`Alternate_ClassicBattery` |
| 古莓 | Paleberry | `Berry` | 固体 | — |
| 固态生物燃料 | Solid Biofuel | `Biofuel` | 固体 | `Biofuel` |
| 电线滑索 | Zipline | `BP_EqDescZipLine` | 固体 | — |
| 拐杖糖棍 | Candy Cane Basher | `BP_EquipmentDescriptorCandyCane` | 固体 | — |
| 防毒面具 | Gas Mask | `BP_EquipmentDescriptorGasmask` | 固体 | — |
| 防护服 | Hazmat Suit | `BP_EquipmentDescriptorHazmatSuit` | 固体 | — |
| 悬浮背包 | Hoverpack | `BP_EquipmentDescriptorHoverPack` | 固体 | — |
| 喷射背包 | Jetpack | `BP_EquipmentDescriptorJetPack` | 固体 | — |
| 银翼跑手 | Blade Runners | `BP_EquipmentDescriptorJumpingStilts` | 固体 | — |
| 诺氏雷筒起爆器 | Nobelisk Detonator | `BP_EquipmentDescriptorNobeliskDetonator` | 固体 | — |
| 物体扫描仪 | Object Scanner | `BP_EquipmentDescriptorObjectScanner` | 固体 | — |
| 步枪 | Rifle | `BP_EquipmentDescriptorRifle` | 固体 | — |
| 异形电击器 | Xeno-Zapper | `BP_EquipmentDescriptorShockShank` | 固体 | — |
| 异形电击棍 | Xeno-Basher | `BP_EquipmentDescriptorStunSpear` | 固体 | — |
| 便携式采矿机 | Portable Miner | `BP_ItemDescriptorPortableMiner` | 固体 | `Alternate_AutomatedMiner` |
| 电缆 | Cable | `Cable` | 固体 | `Cable`、`Alternate_Cable_1`、`Alternate_Cable_2`、`Alternate_CoatedCable` |
| 拐杖糖 | Candy Cane | `CandyCane` | 固体 | `CandyCane` |
| 涡轮步枪弹 | Turbo Rifle Ammo | `CartridgeChaos` | 固体 | `CartridgeChaos`、`CartridgeChaos_Packaged` |
| 追踪步枪弹 | Homing Rifle Ammo | `CartridgeSmartProjectile` | 固体 | `CartridgeSmart` |
| 步枪弹药 | Rifle Ammo | `CartridgeStandard` | 固体 | `Cartridge` |
| 混凝土 | Concrete | `Cement` | 固体 | `Concrete`、`Alternate_Concrete`、`Alternate_RubberConcrete`、`Alternate_WetConcrete` |
| 链锯 | Chainsaw | `Chainsaw` | 固体 | — |
| 电路板 | Circuit Board | `CircuitBoard` | 固体 | `CircuitBoard`、`Alternate_CircuitBoard_1`、`Alternate_CircuitBoard_2`、`Alternate_ElectrodeCircuitBoard` |
| AI限制器 | AI Limiter | `CircuitBoardHighSpeed` | 固体 | `AILimiter`、`Alternate_AILimiter_Plastic` |
| 煤 | Coal | `Coal` | 固体 | `Coal_Iron`、`Coal_Limestone`、`Alternate_Coal_1`、`Alternate_Coal_2` |
| 压缩煤 | Compacted Coal | `CompactedCoal` | 固体 | `Alternate_EnrichedCoal` |
| 计算机 | Computer | `Computer` | 固体 | `Computer`、`Alternate_Computer_1`、`Alternate_Computer_2` |
| 超级计算机 | Supercomputer | `ComputerSuper` | 固体 | `ComputerSuper`、`Alternate_OCSupercomputer`、`Alternate_SuperStateComputer` |
| 冷却系统 | Cooling System | `CoolingSystem` | 固体 | `CoolingSystem`、`Alternate_CoolingDevice` |
| 铜粉 | Copper Powder | `CopperDust` | 固体 | `CopperDust` |
| 铜锭 | Copper Ingot | `CopperIngot` | 固体 | `IngotCopper`、`Alternate_CopperAlloyIngot`、`Alternate_CopperIngot_Leached`、`Alternate_CopperIngot_Tempered`、`Alternate_PureCopperIngot` |
| 铜板 | Copper Sheet | `CopperSheet` | 固体 | `CopperSheet`、`Alternate_SteamedCopperSheet` |
| 蓝色能量蛞蝓 | Blue Power Slug | `Crystal` | 固体 | — |
| 黄色能量蛞蝓 | Yellow Power Slug | `Crystal_mk2` | 固体 | — |
| 紫色能量蛞蝓 | Purple Power Slug | `Crystal_mk3` | 固体 | — |
| 晶体振荡器 | Crystal Oscillator | `CrystalOscillator` | 固体 | `CrystalOscillator`、`Alternate_CrystalOscillator` |
| 能量碎片 | Power Shard | `CrystalShard` | 固体 | `PowerCrystalShard_1`、`PowerCrystalShard_2`、`PowerCrystalShard_3`、`SyntheticPowerShard` |
| 暗物质残渣 | Dark Matter Residue | `DarkEnergy` | 气体 | `DarkEnergy` |
| 暗物质晶体 | Dark Matter Crystal | `DarkMatter` | 固体 | `DarkMatter`、`Alternate_DarkMatter_Crystallization`、`Alternate_DarkMatter_Trap` |
| 钻石 | Diamonds | `Diamond` | 固体 | `Diamond`、`Alternate_Diamond_Cloudy`、`Alternate_Diamond_OilBased`、`Alternate_Diamond_Petroleum`、`Alternate_Diamond_Pink`、`Alternate_Diamond_Turbo` |
| 二氧化硅溶解液 | Dissolved Silica | `DissolvedSilica` | 液体 | `Alternate_Quartz_Purified`（副产物） |
| 电磁控制杆 | Electromagnetic Control Rod | `ElectromagneticControlRod` | 固体 | `ElectromagneticControlRod`、`Alternate_ElectromagneticControlRod_1` |
| 纤维织物 | Fabric | `Fabric` | 固体 | `Fabric`、`Alternate_PolyesterFabric` |
| F金锭 | Ficsite Ingot | `FicsiteIngot` | 固体 | `FicsiteIngot_AL`、`FicsiteIngot_CAT`、`FicsiteIngot_Iron` |
| F金三角 | Ficsite Trigon | `FicsiteMesh` | 固体 | `FicsiteMesh` |
| 镄 | Ficsonium | `Ficsonium` | 固体 | `Ficsonium` |
| 镄燃料棒 | Ficsonium Fuel Rod | `FicsoniumFuelRod` | 固体 | `FicsoniumFuelRod` |
| 气体过滤器 | Gas Filter | `Filter` | 固体 | `FilterGasMask` |
| 甜蜜烟花 | Sweet Fireworks | `Fireworks_Projectile_01` | 固体 | `Fireworks_01` |
| 绚烂烟花 | Fancy Fireworks | `Fireworks_Projectile_02` | 固体 | `Fireworks_02` |
| 闪亮烟花 | Sparkly Fireworks | `Fireworks_Projectile_03` | 固体 | `Fireworks_03` |
| 空桶 | Empty Canister | `FluidCanister` | 固体 | `FluidCanister`、`Alternate_CoatedIronCanister`、`Alternate_SteelCanister` |
| 桶装燃油 | Packaged Fuel | `Fuel` | 固体 | `Fuel`、`Alternate_DilutedPackagedFuel` |
| 空瓶 | Empty Fluid Tank | `GasTank` | 固体 | `GasTank` |
| 生物质 | Biomass | `GenericBiomass` | 固体 | `Biomass_AlienProtein`、`Biomass_Leaves`、`Biomass_Mycelia`、`Biomass_Wood` |
| FICSMAS礼物 | FICSMAS Gift | `Gift` | 固体 | — |
| 钦金锭 | Caterium Ingot | `GoldIngot` | 固体 | `IngotCaterium`、`Alternate_CateriumIngot_Leached`、`Alternate_CateriumIngot_Tempered`、`Alternate_PureCateriumIngot` |
| 工厂车™ | Factory Cart™ | `GolfCart` | 固体 | — |
| 黄金工厂车™ | Golden Factory Cart™ | `GolfCartGold` | 固体 | — |
| 黑火药 | Black Powder | `Gunpowder` | 固体 | `Gunpowder`、`Alternate_Gunpowder_1` |
| 无烟火药 | Smokeless Powder | `GunpowderMK2` | 固体 | `GunpowderMK2` |
| 孵化巢残骸 | Hatcher Remains | `HatcherParts` | 固体 | — |
| 碘化过滤器 | Iodine-Infused Filter | `HazmatFilter` | 固体 | `FilterHazmat` |
| 重油残渣 | Heavy Oil Residue | `HeavyOilResidue` | 液体 | `UnpackageOilResidue`、`Alternate_HeavyOilResidue` |
| 高速连接器 | High-Speed Connector | `HighSpeedConnector` | 固体 | `HighSpeedConnector`、`Alternate_HighSpeedConnector` |
| 急速电线 | Quickwire | `HighSpeedWire` | 固体 | `Quickwire`、`Alternate_Quickwire` |
| 野猪残骸 | Hog Remains | `HogParts` | 固体 | — |
| 电离燃油 | Ionized Fuel | `IonizedFuel` | 气体 | `IonizedFuel`、`UnpackageIonizedFuel`、`Alternate_IonizedFuel_Dark` |
| 铁锭 | Iron Ingot | `IronIngot` | 固体 | `IngotIron`、`Alternate_IngotIron`、`Alternate_IronIngot_Basic`、`Alternate_IronIngot_Leached`、`Alternate_PureIronIngot` |
| 铁板 | Iron Plate | `IronPlate` | 固体 | `IronPlate`、`Alternate_CoatedIronPlate`、`Alternate_SteelCastedPlate` |
| 强化铁板 | Reinforced Iron Plate | `IronPlateReinforced` | 固体 | `IronPlateReinforced`、`Alternate_AdheredIronPlate`、`Alternate_ReinforcedIronPlate_1`、`Alternate_ReinforcedIronPlate_2` |
| 铁棒 | Iron Rod | `IronRod` | 固体 | `IronRod`、`Alternate_AluminumRod`、`Alternate_SteelRod` |
| 螺丝 | Screws | `IronScrew` | 固体 | `Screw`、`Alternate_Screw_2`、`Alternate_Screw` |
| 树叶 | Leaves | `Leaves` | 固体 | — |
| 液态生物燃料 | Liquid Biofuel | `LiquidBiofuel` | 液体 | `LiquidBiofuel`、`UnpackageBioFuel` |
| 燃料 | Fuel | `LiquidFuel` | 液体 | `LiquidFuel`、`UnpackageFuel`、`Alternate_DilutedFuel`、`ResidualFuel` |
| 原油 | Crude Oil | `LiquidOil` | 液体 | `UnpackageOil` |
| 涡轮燃油 | Turbofuel | `LiquidTurboFuel` | 液体 | `UnpackageTurboFuel`、`Alternate_TurboBlendFuel`、`Alternate_TurboHeavyFuel`、`Alternate_Turbofuel` |
| 药用气雾剂 | Medicinal Inhaler | `Medkit` | 固体 | — |
| 模块化框架 | Modular Frame | `ModularFrame` | 固体 | `ModularFrame`、`Alternate_BoltedFrame`、`Alternate_ModularFrame` |
| 焊接模块化框架 | Fused Modular Frame | `ModularFrameFused` | 固体 | `FusedModularFrame`、`Alternate_HeatFusedFrame` |
| 重型模块化框架 | Heavy Modular Frame | `ModularFrameHeavy` | 固体 | `ModularFrameHeavy`、`Alternate_HeavyFlexibleFrame`、`Alternate_ModularFrameHeavy` |
| 无线电控制单元 | Radio Control Unit | `ModularFrameLightweight` | 固体 | `RadioControlUnit`、`Alternate_RadioControlSystem`、`Alternate_RadioControlUnit_1` |
| 马达 | Motor | `Motor` | 固体 | `Motor`、`Alternate_ElectricMotor`、`Alternate_Motor_1` |
| 涡轮马达 | Turbo Motor | `MotorLightweight` | 固体 | `MotorTurbo`、`Alternate_TurboMotor_1`、`Alternate_TurboPressureMotor` |
| 菌丝体 | Mycelia | `Mycelia` | 固体 | — |
| 硝酸 | Nitric Acid | `NitricAcid` | 液体 | `NitricAcid`、`UnpackageNitricAcid` |
| 氮气 | Nitrogen Gas | `NitrogenGas` | 气体 | `Nitrogen_Bauxite`、`Nitrogen_Caterium`、`UnpackageNitrogen` |
| 集束诺氏雷筒 | Cluster Nobelisk | `NobeliskCluster` | 固体 | `NobeliskCluster` |
| 诺氏雷筒 | Nobelisk | `NobeliskExplosive` | 固体 | `Nobelisk` |
| 毒气诺石 | Gas Nobelisk | `NobeliskGas` | 固体 | `NobeliskGas` |
| 核子诺氏雷筒 | Nuke Nobelisk | `NobeliskNuke` | 固体 | `NobeliskNuke` |
| 脉冲诺石 | Pulse Nobelisk | `NobeliskShockwave` | 固体 | `NobeliskShockwave` |
| 非裂变贫铀 | Non-Fissile Uranium | `NonFissibleUranium` | 固体 | `NonFissileUranium`、`Alternate_FertileUranium` |
| 铀燃料棒 | Uranium Fuel Rod | `NuclearFuelRod` | 固体 | `NuclearFuelRod`、`Alternate_NuclearFuelRod_1` |
| 铀废料 | Uranium Waste | `NuclearWaste` | 固体 | — |
| 绿石果 | Beryl Nut | `Nut` | 固体 | — |
| 铝土矿 | Bauxite | `OreBauxite` | 固体 | `Bauxite_Caterium`、`Bauxite_Copper` |
| 铜矿石 | Copper Ore | `OreCopper` | 固体 | `Copper_Quartz`、`Copper_Sulfur` |
| 钦金矿石 | Caterium Ore | `OreGold` | 固体 | `Caterium_Copper`、`Caterium_Quartz` |
| 铁矿石 | Iron Ore | `OreIron` | 固体 | `Iron_Limestone` |
| 铀 | Uranium | `OreUranium` | 固体 | `Uranium_Bauxite` |
| 桶装氧化铝溶液 | Packaged Alumina Solution | `PackagedAlumina` | 固体 | `PackagedAlumina` |
| 桶装液态生物燃料 | Packaged Liquid Biofuel | `PackagedBiofuel` | 固体 | `PackagedBiofuel` |
| 桶装电离燃油 | Packaged Ionized Fuel | `PackagedIonizedFuel` | 固体 | `PackagedIonizedFuel` |
| 桶装硝酸 | Packaged Nitric Acid | `PackagedNitricAcid` | 固体 | `PackagedNitricAcid` |
| 瓶装氮气 | Packaged Nitrogen Gas | `PackagedNitrogenGas` | 固体 | `PackagedNitrogen` |
| 桶装原油 | Packaged Oil | `PackagedOil` | 固体 | `PackagedCrudeOil` |
| 桶装重油残渣 | Packaged Heavy Oil Residue | `PackagedOilResidue` | 固体 | `PackagedOilResidue` |
| 桶装火箭燃料 | Packaged Rocket Fuel | `PackagedRocketFuel` | 固体 | `PackagedRocketFuel` |
| 桶装硫酸 | Packaged Sulfuric Acid | `PackagedSulfuricAcid` | 固体 | `PackagedSulfuricAcid` |
| 桶装水 | Packaged Water | `PackagedWater` | 固体 | `PackagedWater` |
| 降落伞 | Parachute | `Parachute` | 固体 | — |
| 石油焦 | Petroleum Coke | `PetroleumCoke` | 固体 | `PetroleumCoke` |
| 塑料 | Plastic | `Plastic` | 固体 | `Plastic`、`Alternate_Plastic_1`、`ResidualPlastic` |
| 封装钚棒 | Encased Plutonium Cell | `PlutoniumCell` | 固体 | `PlutoniumCell`、`Alternate_InstantPlutoniumCell` |
| 钚燃料棒 | Plutonium Fuel Rod | `PlutoniumFuelRod` | 固体 | `PlutoniumFuelRod`、`Alternate_PlutoniumFuelUnit` |
| 钚丸 | Plutonium Pellet | `PlutoniumPellet` | 固体 | `Plutonium` |
| 钚废料 | Plutonium Waste | `PlutoniumWaste` | 固体 | — |
| 聚合树脂 | Polymer Resin | `PolymerResin` | 固体 | `Alternate_PolymerResin` |
| 封压方块 | Pressure Conversion Cube | `PressureConversionCube` | 固体 | `PressureConversionCube` |
| 激发态光子物质 | Excited Photonic Matter | `QuantumEnergy` | 气体 | `QuantumEnergy` |
| 叠加态振荡器 | Superposition Oscillator | `QuantumOscillator` | 固体 | `SuperpositionOscillator` |
| 石英晶体 | Quartz Crystal | `QuartzCrystal` | 固体 | `QuartzCrystal`、`Alternate_PureQuartzCrystal`、`Alternate_Quartz_Fused`、`Alternate_Quartz_Purified` |
| 粗石英 | Raw Quartz | `RawQuartz` | 固体 | `Quartz_Bauxite`、`Quartz_Coal` |
| 爆炸铁棒弹 | Explosive Rebar | `Rebar_Explosive` | 固体 | `Rebar_Explosive` |
| 散射铁棒弹 | Shatter Rebar | `Rebar_Spreadshot` | 固体 | `Rebar_Spreadshot` |
| 电击铁棒弹 | Stun Rebar | `Rebar_Stunshot` | 固体 | `Rebar_Stunshot` |
| 射棒枪 | Rebar Gun | `RebarGunProjectile` | 固体 | — |
| 火箭燃料 | Rocket Fuel | `RocketFuel` | 气体 | `RocketFuel`、`UnpackageRocketFuel`、`Alternate_RocketFuel_Nitro` |
| 转子 | Rotor | `Rotor` | 固体 | `Rotor`、`Alternate_CopperRotor`、`Alternate_Rotor` |
| 橡胶 | Rubber | `Rubber` | 固体 | `Rubber`、`Alternate_RecycledRubber`、`ResidualRubber` |
| SAM物质 | SAM | `SAM` | 固体 | — |
| SAM物质波波动器 | SAM Fluctuator | `SAMFluctuator` | 固体 | `SAMFluctuator` |
| 活性SAM物质 | Reanimated SAM | `SAMIngot` | 固体 | `IngotSAM` |
| 培根伞菌 | Bacon Agaric | `Shroom` | 固体 | — |
| 二氧化硅 | Silica | `Silica` | 固体 | `Silica`、`Alternate_Silica`、`Alternate_Silica_Distilled` |
| 奇点电池 | Singularity Cell | `SingularityCell` | 固体 | `SingularityCell` |
| FICSMAS“真雪” | FICSMAS Actual Snow | `Snow` | 固体 | `Snow` |
| 雪球 | Snowball | `SnowballProjectile` | 固体 | `Snowball` |
| 智能嵌板 | Smart Plating | `SpaceElevatorPart_1` | 固体 | `SpaceElevatorPart_1`、`Alternate_PlasticSmartPlating` |
| 生化塑造器 | Biochemical Sculptor | `SpaceElevatorPart_10` | 固体 | `SpaceElevatorPart_10` |
| 弹道跃迁引擎 | Ballistic Warp Drive | `SpaceElevatorPart_11` | 固体 | `SpaceElevatorPart_11` |
| AI拓展伺服器 | AI Expansion Server | `SpaceElevatorPart_12` | 固体 | `SpaceElevatorPart_12` |
| 多功能框架 | Versatile Framework | `SpaceElevatorPart_2` | 固体 | `SpaceElevatorPart_2`、`Alternate_FlexibleFramework` |
| 自动化线缆 | Automated Wiring | `SpaceElevatorPart_3` | 固体 | `SpaceElevatorPart_3`、`Alternate_HighSpeedWiring` |
| 模块化引擎 | Modular Engine | `SpaceElevatorPart_4` | 固体 | `SpaceElevatorPart_4` |
| 自适应控制单元 | Adaptive Control Unit | `SpaceElevatorPart_5` | 固体 | `SpaceElevatorPart_5` |
| 磁场发生器 | Magnetic Field Generator | `SpaceElevatorPart_6` | 固体 | `SpaceElevatorPart_6` |
| 组装编导系统 | Assembly Director System | `SpaceElevatorPart_7` | 固体 | `SpaceElevatorPart_7` |
| 热能推进火箭 | Thermal Propulsion Rocket | `SpaceElevatorPart_8` | 固体 | `SpaceElevatorPart_8` |
| 核意面 | Nuclear Pasta | `SpaceElevatorPart_9` | 固体 | `SpaceElevatorPart_9` |
| 铁棒弹 | Iron Rebar | `SpikedRebar` | 固体 | `SpikedRebar` |
| 吐射兽残骸 | Spitter Remains | `SpitterParts` | 固体 | — |
| 定子 | Stator | `Stator` | 固体 | `Stator`、`Alternate_Stator` |
| 钢锭 | Steel Ingot | `SteelIngot` | 固体 | `IngotSteel`、`Alternate_CokeSteelIngot`、`Alternate_IngotSteel_1`、`Alternate_IngotSteel_2` |
| 钢管 | Steel Pipe | `SteelPipe` | 固体 | `SteelPipe`、`Alternate_SteelPipe_Iron`、`Alternate_SteelPipe_Molded` |
| 钢梁 | Steel Beam | `SteelPlate` | 固体 | `SteelBeam`、`Alternate_SteelBeam_Aluminum`、`Alternate_SteelBeam_Molded` |
| 钢筋混凝土梁 | Encased Industrial Beam | `SteelPlateReinforced` | 固体 | `EncasedIndustrialBeam`、`Alternate_EncasedIndustrialBeam` |
| 刺蛛残骸 | Stinger Remains | `StingerParts` | 固体 | — |
| 石灰石 | Limestone | `Stone` | 固体 | `Limestone_Sulfur` |
| 硫磺 | Sulfur | `Sulfur` | 固体 | `Sulfur_Coal`、`Sulfur_Iron` |
| 硫酸 | Sulfuric Acid | `SulfuricAcid` | 液体 | `SulfuricAcid`、`UnpackageSulfuricAcid` |
| 神经量子处理器 | Neural-Quantum Processor | `TemporalProcessor` | 固体 | `TemporalProcessor` |
| 时间水晶 | Time Crystal | `TimeCrystal` | 固体 | `TimeCrystal` |
| 桶装涡轮燃油 | Packaged Turbofuel | `TurboFuel` | 固体 | `PackagedTurboFuel` |
| 封装铀棒 | Encased Uranium Cell | `UraniumCell` | 固体 | `UraniumCell`、`Alternate_UraniumCell_1` |
| 索莫晶体 | Somersloop | `WAT1` | 固体 | — |
| 梅瑟尔球 | Mercer Sphere | `WAT2` | 固体 | — |
| 水 | Water | `Water` | 液体 | `UnpackageWater` |
| 电线 | Wire | `Wire` | 固体 | `Wire`、`Alternate_FusedWire`、`Alternate_Wire_1`、`Alternate_Wire_2` |
| 木材 | Wood | `Wood` | 固体 | — |
| 红色FICSMAS挂饰 | Red FICSMAS Ornament | `XmasBall1` | 固体 | `XmasBall1` |
| 蓝色FICSMAS挂饰 | Blue FICSMAS Ornament | `XmasBall2` | 固体 | `XmasBall2` |
| 铜制FICSMAS挂饰 | Copper FICSMAS Ornament | `XmasBall3` | 固体 | `XmasBall3` |
| 铁制FICSMAS挂饰 | Iron FICSMAS Ornament | `XmasBall4` | 固体 | `XmasBall4` |
| FICSMAS挂饰套装 | FICSMAS Ornament Bundle | `XmasBallCluster` | 固体 | `XmasBallCluster` |
| FICSMAS蝴蝶结 | FICSMAS Bow | `XmasBow` | 固体 | `XmasBow` |
| FICSMAS圣诞树枝 | FICSMAS Tree Branch | `XmasBranch` | 固体 | `XmasBranch` |
| FICSMAS数据卡带第1天 | FICSMAS Data Cartridge Day 1 | `XmasDataCartridge1` | 固体 | — |
| FICSMAS数据卡带第4天 | FICSMAS Data Cartridge Day 4 | `XmasDataCartridge2` | 固体 | — |
| FICSMAS数据卡带第8天 | FICSMAS Data Cartridge Day 8 | `XmasDataCartridge3` | 固体 | — |
| FICSMAS数据卡带第14天 | FICSMAS Data Cartridge Day 14 | `XmasDataCartridge4` | 固体 | — |
| FICSMAS许愿星 | FICSMAS Wonder Star | `XmasStar` | 固体 | `XmasStar` |
| FICSMAS圣诞花环 | FICSMAS Wreath | `XmasWreath` | 固体 | `XmasWreath` |
