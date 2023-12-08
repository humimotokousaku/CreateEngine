#pragma once
#include "IScene.h"
#include "../components/Input.h"
#include "../object/Sprite.h"
#include "../object/Triangle.h"
#include "../base/WorldTransform.h"
#include "../base/ViewProjection.h"
#include "../base/Model.h"
#include "../Manager/PipelineManager.h"
#include "../Plane.h"

class GameManager;

class TitleScene : public IScene
{
public:
	void Initialize() override;
	void Update() override;
	void Draw() override;
	void Finalize()override;
private:
	const static int kMaxObject = 2;
	Model* fence_;
	Plane* plane_;
	std::unique_ptr<Triangle> triangle_;
	Input* input_;
	WorldTransform worldTransform_[kMaxObject];
	ViewProjection viewProjection_;

	int blendMode_ = kBlendModeNone;
};