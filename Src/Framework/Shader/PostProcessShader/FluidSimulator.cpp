#include "FluidSimulator.h"

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// シェーダーとRTの生成
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
bool FluidSimulator::Init(int simWidth)
{
	// 全画面描画用のVSとInputLayout(KdPostProcessShaderと同じVSを使う)
	{
#include "KdPostProcessShader_VS.shaderInc"

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreateVertexShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_VS)))
		{
			assert(0 && "頂点シェーダー作成失敗(Fluid)");
			Release();
			return false;
		}

		std::vector<D3D11_INPUT_ELEMENT_DESC> layout = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreateInputLayout(
			&layout[0], (UINT)layout.size(), compiledBuffer, sizeof(compiledBuffer), &m_inputLayout)))
		{
			assert(0 && "CreateInputLayout失敗(Fluid)");
			Release();
			return false;
		}
	}

	{
#include "KdFluid_PS.shaderInc"

		if (FAILED(KdDirect3D::Instance().WorkDev()->CreatePixelShader(compiledBuffer, sizeof(compiledBuffer), nullptr, &m_PS)))
		{
			assert(0 && "ピクセルシェーダー作成失敗(Fluid)");
			Release();
			return false;
		}
	}

	m_cb.Create();

	// 格子を画面の縦横比に合わせる(テクセルが正方形になり、速度をtexel単位で等方に扱える)
	const std::shared_ptr<KdTexture>& backBuffer = KdDirect3D::Instance().GetBackBuffer();
	float aspect = static_cast<float>(backBuffer->GetWidth()) / backBuffer->GetHeight();

	m_simWidth = simWidth;
	m_simHeight = (std::max)(1, static_cast<int>(simWidth / aspect + 0.5f));

	m_velocity.Create(m_simWidth, m_simHeight, DXGI_FORMAT_R16G16_FLOAT);
	m_density.Create(m_simWidth, m_simHeight, DXGI_FORMAT_R16_FLOAT);

	cbFluid& cb = m_cb.Work();
	cb.TexelSize = { 1.0f / m_simWidth, 1.0f / m_simHeight };
	cb.Aspect = aspect;

	// 画面全体に書き込む用の頂点
	m_screenVert[0] = { {-1,-1,0}, {0, 1} };
	m_screenVert[1] = { {-1, 1,0}, {0, 0} };
	m_screenVert[2] = { { 1,-1,0}, {1, 1} };
	m_screenVert[3] = { { 1, 1,0}, {1, 0} };

	m_velocity.Clear();
	m_density.Clear();

	return true;
}

void FluidSimulator::Release()
{
	KdSafeRelease(m_VS);
	KdSafeRelease(m_inputLayout);
	KdSafeRelease(m_PS);

	m_cb.Release();

	m_velocity.Release();
	m_density.Release();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 1フレーム分のシミュレーション
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void FluidSimulator::Step(float deltaTime)
{
	if (!m_PS)
	{
		m_splats.clear();
		return;
	}

	KdShaderManager& shaderMgr = KdShaderManager::Instance();
	ID3D11DeviceContext* devCon = KdDirect3D::Instance().WorkDevContext();

	if (shaderMgr.SetVertexShader(m_VS))
	{
		devCon->IASetInputLayout(m_inputLayout);
	}
	shaderMgr.SetPixelShader(m_PS);
	devCon->PSSetConstantBuffers(0, 1, m_cb.GetAddress());

	// 既定のサンプラーは補間なしのため、線形補間+端クランプへ切り替える
	shaderMgr.ChangeSamplerState(KdSamplerState::Linear_Clamp);

	float dt = std::clamp(deltaTime, kMinDeltaTime, kMaxDeltaTime);
	m_cb.Work().DeltaTime = dt;

	ApplySplats(dt);

	// 速度の自己移流 → 密度を速度で移流(ステップ4でこの間に圧力投影が入る)
	Advect(m_velocity, m_velocity, kVelocityDissipation);
	Advect(m_velocity, m_density, kDensityDissipation);

	shaderMgr.UndoSamplerState();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 移流：velocityで逆向きにたどった位置の値をfieldへ書き、fieldを入れ替える
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void FluidSimulator::Advect(FluidDoubleRT& velocity, FluidDoubleRT& field, float dissipation)
{
	m_cb.Work().Dissipation = dissipation;

	DrawPass(Pass::Advect, velocity.Read().m_RTTexture.get(), field.Read().m_RTTexture.get(), field.Write());

	field.Swap();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 墨の入力を記録する(反映は次のStep())
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void FluidSimulator::AddSplat(const Math::Vector2& posUV, const Math::Vector2& deltaUV, float radius, float density)
{
	if (!m_PS || radius <= 0.0f) { return; }

	m_splats.push_back({ posUV, deltaUV, radius, density });
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 記録した入力を、移動量から求めた速度と濃度として加える
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void FluidSimulator::ApplySplats(float dt)
{
	for (const SplatRequest& s : m_splats)
	{
		// 速度はtexel/秒。急な大移動で暴れないよう上限を設ける
		Math::Vector2 vel = { s.Delta.x * m_simWidth / dt, s.Delta.y * m_simHeight / dt };
		float speed = vel.Length();
		if (speed > kMaxSplatSpeed) { vel *= kMaxSplatSpeed / speed; }

		// 高速移動で点線にならないよう、軌跡を半径の半分刻みで補間する
		Math::Vector2 aspectDelta = { s.Delta.x * m_cb.Get().Aspect, s.Delta.y };
		int n = std::clamp(static_cast<int>(std::ceil(aspectDelta.Length() / (s.Radius * 0.5f))), 1, kMaxSplatSubdivision);

		Math::Vector2 prev = s.Pos - s.Delta;

		for (int i = 0; i < n; ++i)
		{
			Math::Vector2 pos = prev + s.Delta * (static_cast<float>(i + 1) / n);

			// 分割数で割り、軌跡全体の入力量が分割数に依らないようにする
			Splat(m_velocity, Pass::SplatVelocity, pos, s.Radius, { vel.x / n, vel.y / n, 0.0f });
			Splat(m_density, Pass::SplatDensity, pos, s.Radius, { s.Density / n, 0.0f, 0.0f });
		}
	}

	m_splats.clear();
}

void FluidSimulator::Splat(FluidDoubleRT& field, Pass pass, const Math::Vector2& posUV, float radius, const Math::Vector3& value)
{
	cbFluid& cb = m_cb.Work();
	cb.SplatPos = posUV;
	cb.SplatRadius = radius;
	cb.SplatValue = value;

	DrawPass(pass, field.Read().m_RTTexture.get(), nullptr, field.Write());

	field.Swap();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 全画面描画1回分。RT・ビューポートは描画後に元へ戻す
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void FluidSimulator::DrawPass(Pass pass, KdTexture* src0, KdTexture* src1, KdRenderTargetPack& dst)
{
	ID3D11DeviceContext* devCon = KdDirect3D::Instance().WorkDevContext();

	m_cb.Work().Pass = static_cast<int>(pass);
	m_cb.Write();

	KdRenderTargetChanger rtChanger;
	if (!rtChanger.ChangeRenderTarget(dst)) { return; }

	if (src0) { devCon->PSSetShaderResources(0, 1, src0->WorkSRViewAddress()); }
	if (src1) { devCon->PSSetShaderResources(1, 1, src1->WorkSRViewAddress()); }

	KdDirect3D::Instance().DrawVertices(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, 4, &m_screenVert[0], sizeof(Vertex));

	// 次のパスで書き込み先になるRTがSRVに残らないよう外す
	ID3D11ShaderResourceView* nullSRV[2] = {};
	devCon->PSSetShaderResources(0, 2, nullSRV);

	rtChanger.UndoRenderTarget();
}
