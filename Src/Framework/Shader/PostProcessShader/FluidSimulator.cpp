#include "FluidSimulator.h"

#include <DirectXPackedVector.h>

namespace
{
	// 現在バインドされているRT(0番)のサイズを取得する
	bool GetBoundRTSize(ID3D11DeviceContext* devCon, float& width, float& height)
	{
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
		devCon->OMGetRenderTargets(1, rtv.GetAddressOf(), nullptr);
		if (!rtv) { return false; }

		Microsoft::WRL::ComPtr<ID3D11Resource> resource;
		rtv->GetResource(resource.GetAddressOf());

		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
		if (FAILED(resource.As(&texture))) { return false; }

		D3D11_TEXTURE2D_DESC desc;
		texture->GetDesc(&desc);

		width = static_cast<float>(desc.Width);
		height = static_cast<float>(desc.Height);

		return true;
	}
}

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
	m_pressure.Create(m_simWidth, m_simHeight, DXGI_FORMAT_R16_FLOAT);
	m_divergence.CreateRenderTarget(m_simWidth, m_simHeight, false, DXGI_FORMAT_R16_FLOAT);
	m_curl.CreateRenderTarget(m_simWidth, m_simHeight, false, DXGI_FORMAT_R16_FLOAT);

	// 占有率の計測用：縮小グリッドと、CPUへ読み戻すためのステージング
	m_gridWidth = kCoverageGridWidth;
	m_gridHeight = (std::max)(1, static_cast<int>(kCoverageGridWidth / aspect + 0.5f));
	m_coverageGrid.CreateRenderTarget(m_gridWidth, m_gridHeight, false, DXGI_FORMAT_R16_FLOAT);

	Microsoft::WRL::ComPtr<ID3D11Resource> gridResource;
	m_coverageGrid.m_RTTexture->WorkRTView()->GetResource(gridResource.GetAddressOf());
	gridResource.As(&m_coverageTexture);

	D3D11_TEXTURE2D_DESC stagingDesc;
	m_coverageTexture->GetDesc(&stagingDesc);
	stagingDesc.Usage = D3D11_USAGE_STAGING;
	stagingDesc.BindFlags = 0;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingDesc.MiscFlags = 0;

	for (int i = 0; i < kStagingCount; ++i)
	{
		if (FAILED(KdDirect3D::Instance().WorkDev()->CreateTexture2D(&stagingDesc, nullptr, m_staging[i].GetAddressOf())))
		{
			assert(0 && "ステージングテクスチャ作成失敗(Fluid)");
			Release();
			return false;
		}
	}

	cbFluid& cb = m_cb.Work();
	cb.TexelSize = { 1.0f / m_simWidth, 1.0f / m_simHeight };
	cb.Aspect = aspect;
	cb.Diffusion = kDensityDiffusion;
	cb.Vorticity = kVorticity;
	cb.StreamSpeed = kStreamSpeed;
	cb.StreamDensity = kStreamDensity;

	// 画面全体に書き込む用の頂点
	m_screenVert[0] = { {-1,-1,0}, {0, 1} };
	m_screenVert[1] = { {-1, 1,0}, {0, 0} };
	m_screenVert[2] = { { 1,-1,0}, {1, 1} };
	m_screenVert[3] = { { 1, 1,0}, {1, 0} };

	m_velocity.Clear();
	m_density.Clear();
	m_pressure.Clear();

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
	m_pressure.Release();
	m_divergence.m_RTTexture.reset();
	m_curl.m_RTTexture.reset();

	m_coverageGrid.m_RTTexture.reset();
	m_coverageTexture.Reset();
	for (auto& staging : m_staging) { staging.Reset(); }
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 濃度を墨と紙の見た目に変換して、現在のレンダーターゲット全体へ描く
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void FluidSimulator::Draw()
{
	if (!m_PS) { return; }

	KdShaderManager& shaderMgr = KdShaderManager::Instance();
	ID3D11DeviceContext* devCon = KdDirect3D::Instance().WorkDevContext();

	BindPipeline();

	// 直前の描画で小さなビューポートが残っていても、描画先の全面に描く
	UINT numVP = 1;
	D3D11_VIEWPORT oldVP = {};
	devCon->RSGetViewports(&numVP, &oldVP);

	float width = 0.0f, height = 0.0f;
	bool changedVP = GetBoundRTSize(devCon, width, height);
	if (changedVP)
	{
		D3D11_VIEWPORT vp = { 0.0f, 0.0f, width, height, 0.0f, 1.0f };
		devCon->RSSetViewports(1, &vp);
	}

	shaderMgr.ChangeSamplerState(KdSamplerState::Linear_Clamp);
	shaderMgr.ChangeDepthStencilState(KdDepthStencilState::ZDisable);

	DrawQuad(Pass::Display, m_density.Read().m_RTTexture.get(), nullptr);

	shaderMgr.UndoDepthStencilState();
	shaderMgr.UndoSamplerState();

	if (changedVP) { devCon->RSSetViewports(1, &oldVP); }
}

void FluidSimulator::BindPipeline()
{
	ID3D11DeviceContext* devCon = KdDirect3D::Instance().WorkDevContext();

	if (KdShaderManager::Instance().SetVertexShader(m_VS))
	{
		devCon->IASetInputLayout(m_inputLayout);
	}
	KdShaderManager::Instance().SetPixelShader(m_PS);
	devCon->PSSetConstantBuffers(0, 1, m_cb.GetAddress());
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

	BindPipeline();

	// 既定のサンプラーは補間なしのため、線形補間+端クランプへ切り替える
	shaderMgr.ChangeSamplerState(KdSamplerState::Linear_Clamp);

	float dt = std::clamp(deltaTime, kMinDeltaTime, kMaxDeltaTime);
	m_cb.Work().DeltaTime = dt;

	UpdateStream(dt);
	ApplySplats(dt);

	// 速度の自己移流 → 渦度強調 → 圧力投影で非圧縮にする → 入口から速度を流し込む
	Advect(m_velocity, m_velocity, kVelocityDissipation);
	AddVorticity();
	Project();
	Emit(Pass::EmitVelocity, m_velocity);

	// 密度を移流して滲ませる → 入口から墨を流し込む
	Advect(m_velocity, m_density, kDensityDissipation);
	Diffuse(m_density);
	Emit(Pass::EmitDensity, m_density);

	// 次の制御に使う、墨の占有率を計測する
	MeasureCoverage();

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
// 圧力投影：発散(湧き出し・吸い込み)を圧力の勾配で打ち消し、流れが回り込むようにする
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void FluidSimulator::Project()
{
	DrawPass(Pass::Divergence, m_velocity.Read().m_RTTexture.get(), nullptr, m_divergence);

	// 前フレームの圧力から始めるので、反復が少なくても収束しやすい
	for (int i = 0; i < kJacobiIterations; ++i)
	{
		DrawPass(Pass::Jacobi, m_pressure.Read().m_RTTexture.get(), m_divergence.m_RTTexture.get(), m_pressure.Write());
		m_pressure.Swap();
	}

	DrawPass(Pass::Gradient, m_pressure.Read().m_RTTexture.get(), m_velocity.Read().m_RTTexture.get(), m_velocity.Write());
	m_velocity.Swap();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 渦度強調：渦の中心へ向かう方向と直交する力を加え、数値拡散で弱まる渦を保つ
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void FluidSimulator::AddVorticity()
{
	DrawPass(Pass::Curl, m_velocity.Read().m_RTTexture.get(), nullptr, m_curl);

	DrawPass(Pass::VorticityForce, m_curl.m_RTTexture.get(), m_velocity.Read().m_RTTexture.get(), m_velocity.Write());
	m_velocity.Swap();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 拡散：隣のマスとなじませる(合計は変わらず、輪郭だけが滲む)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void FluidSimulator::Diffuse(FluidDoubleRT& field)
{
	DrawPass(Pass::Diffuse, field.Read().m_RTTexture.get(), nullptr, field.Write());
	field.Swap();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// ストリーム：左端の入口から、一本の墨の線を流し込み続ける(出口は右端)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void FluidSimulator::UpdateStream(float dt)
{
	ReadCoverage();

	m_streamTime += dt;

	// 占有率が目標に足りなければ帯を太く、多ければ細くする
	if (m_coverageValid)
	{
		float error = kTargetCoverage - m_coverage;

		m_streamHalfWidth = std::clamp(m_streamHalfWidth + error * kStreamWidthGain * dt, kStreamWidthMin, kStreamWidthMax);
	}

	// 入口の位置と向きをゆっくり揺らし、線がうねるようにする
	cbFluid& cb = m_cb.Work();
	cb.StreamCenterY = 0.5f + kStreamCenterSwing * std::sin(m_streamTime * kStreamCenterSpeed);
	cb.StreamHalfWidth = m_streamHalfWidth;
	cb.StreamAngle = kStreamAngleSwing * std::sin(m_streamTime * kStreamAngleSpeed);
}

void FluidSimulator::Emit(Pass pass, FluidDoubleRT& field)
{
	DrawPass(pass, field.Read().m_RTTexture.get(), nullptr, field.Write());
	field.Swap();
}

// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
// 占有率の計測：濃度を縮小グリッドへ平均してステージングへコピーする(読み取りは数フレーム後)
// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// ///// /////
void FluidSimulator::MeasureCoverage()
{
	if (++m_measureFrame < kMeasureInterval) { return; }
	m_measureFrame = 0;

	// 読み取りが追いついていない間は、未読のバッファを上書きしないよう見送る
	if (m_stagingPending >= kStagingCount) { return; }

	m_cb.Work().DstSize = { static_cast<float>(m_gridWidth), static_cast<float>(m_gridHeight) };
	DrawPass(Pass::Coverage, m_density.Read().m_RTTexture.get(), nullptr, m_coverageGrid);

	KdDirect3D::Instance().WorkDevContext()->CopyResource(m_staging[m_stagingWrite].Get(), m_coverageTexture.Get());

	m_stagingWrite = (m_stagingWrite + 1) % kStagingCount;
	++m_stagingPending;
}

// コピーが終わったものを古い順に読み取り、最新の値を占有率とする(GPUは待たない)
void FluidSimulator::ReadCoverage()
{
	ID3D11DeviceContext* devCon = KdDirect3D::Instance().WorkDevContext();

	while (m_stagingPending > 0)
	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		if (FAILED(devCon->Map(m_staging[m_stagingRead].Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped)))
		{
			return;
		}

		double sum = 0.0;

		for (int y = 0; y < m_gridHeight; ++y)
		{
			const uint16_t* row = reinterpret_cast<const uint16_t*>(static_cast<const uint8_t*>(mapped.pData) + y * mapped.RowPitch);

			for (int x = 0; x < m_gridWidth; ++x)
			{
				sum += DirectX::PackedVector::XMConvertHalfToFloat(row[x]);
			}
		}

		devCon->Unmap(m_staging[m_stagingRead].Get(), 0);

		m_coverage = static_cast<float>(sum / (m_gridWidth * m_gridHeight));
		m_coverageValid = true;

		m_stagingRead = (m_stagingRead + 1) % kStagingCount;
		--m_stagingPending;
	}
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
	KdRenderTargetChanger rtChanger;
	if (!rtChanger.ChangeRenderTarget(dst)) { return; }

	DrawQuad(pass, src0, src1);

	rtChanger.UndoRenderTarget();
}

void FluidSimulator::DrawQuad(Pass pass, KdTexture* src0, KdTexture* src1)
{
	ID3D11DeviceContext* devCon = KdDirect3D::Instance().WorkDevContext();

	m_cb.Work().Pass = static_cast<int>(pass);
	m_cb.Write();

	if (src0) { devCon->PSSetShaderResources(0, 1, src0->WorkSRViewAddress()); }
	if (src1) { devCon->PSSetShaderResources(1, 1, src1->WorkSRViewAddress()); }

	KdDirect3D::Instance().DrawVertices(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, 4, &m_screenVert[0], sizeof(Vertex));

	// 次のパスで書き込み先になるRTがSRVに残らないよう外す
	ID3D11ShaderResourceView* nullSRV[2] = {};
	devCon->PSSetShaderResources(0, 2, nullSRV);
}
