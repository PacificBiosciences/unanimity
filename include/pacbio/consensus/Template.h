
#pragma once

namespace PacBio {
namespace Consensus {

class AbstractTemplate
{
public:
    AbstractTemplate(std::unique_ptr<ModelConfig>&& cfg)
        : cfg_{cfg}
    { }

    const TemplatePosition& operator[](size_t i) const = 0;

    // virtual mutations (for mutation testing purposes)
    virtual bool IsMutated() const = 0;
    virtual void Mutate(const Mutation& m) = 0;
    virtual void Reset() = 0;

    // actually apply mutations
    virtual void ApplyMutation(const Mutation& mut) = 0;
    virtual void ApplyMutations(const std::vector<Mutation>& muts) = 0;

    inline
    std::tuple<double, double> NormalParameters(size_t start, size_t end) const
    {
        double mean = 0.0, var = 0.0;

        for (size_t i = start, i < end; ++i)
        {
            double m, v;
            std::tie(m, v) = SiteNormalParameters((*this)[i], this->Eps());
            mean += m;
            var  += v;
        }

        return std::make_tuple(mean, var);
    }

    double CovariatePr(MoveType move, uint8_t cov) const
    {
        return cfg_->CovariatePr(move, cov);        
    }

    double MiscallPr(char from, char to) const
    {
        return cfg_->MiscallPr(from, to);
    }

protected:
    std::unique_ptr<ModelConfig> cfg_;

private:
    std::tuple<double, double> SiteNormalParameters(const TemplatePosition& tpos
                                                    const double eps)
    {
        const double p_m = params.Match,     l_m = std::log(p_m),  l2_m = l_m * l_m;
        const double p_d = params.Deletion,  l_d = std::log(p_d),  l2_d = l_d * l_d;
        const double p_b = params.Branch,    l_b = std::log(p_b),  l2_b = l_b * l_b;
        const double p_s = params.Stick,     l_s = std::log(p_s),  l2_s = l_s * l_s;

        const double lgThird = -std::log(3.0);
        const double E_M = (1.0 - eps) * 0.0 + eps * lgThird,  E2_M = eps * lgThird * lgThird;
        const double E_D = 0.0,                                E2_D = E_D * E_D;
        const double E_B = 0.0,                                E2_B = E_B * E_B;
        const double E_S = lgThird,                            E2_S = E_S * E_S;

        auto ENN = [=] (const double l_m, const double l_d, const double l_b, const double l_s,
                        const double E_M, const double E_D, const double E_B, const double E_S)
        {
            const double E_MD = (l_m + E_M) * p_m / (p_m + p_d) + (l_d + E_D) * p_d / (p_m + p_d);
            const double E_I  = (l_b + E_B) * p_b / (p_b + p_s) + (l_s + E_S) * p_s / (p_b + p_s);
            const double E_BS = E_I * (p_s + p_b) / (p_m + p_d);
            return E_MD + E_BS;
        };

        const double mean = ENN(l_m, l_d, l_b, l_s, E_M, E_D, E_B, E_S);
        const double var  = ENN(l2_m, l2_d, l2_b, l2_s, E2_M, E2_D, E2_B, E2_S) - mean * mean;

        return std::make_tuple(mean, var);
    }
};

class Template : public AbstractTemplate
{
public:
    Template(const std::string& tpl, const ModelConfig& cfg, const SNR& snr)
        : AbstractTemplate(cfg)
        , tpl_{cfg_->Populate(tpl, snr)}
    { }

    const TemplatePosition& operator[](size_t i) const
    { return tpl_[i]; }

    bool IsMutated() const;
    void Mutate(const Mutation& mut);
    void Reset();

    void ApplyMutation(const Mutation& mut);
    void ApplyMutations(const std::vector<Mutation>& muts);

private:
    std::vector<TemplatePosition> tpl_;

    friend class VirtualTemplate;
};

class VirtualTemplate : public AbstractTemplate
{
    VirtualTemplate(const Template& master)
        : AbstractTemplate(master.cfg_)
        , master_{master}
    { }

    const TemplatePosition& operator[](size_t i) const
    { return master_[i]; }

    bool IsMutated() const
    {
        return master_.IsMutated();
    }

    void Mutate(const Mutation& m)
    {
        if (master_.IsMutated())
            throw std::runtime_error("virtual template badness");
    }

    void Reset()
    {
        if (!master_.IsMutated())
            throw std::runtime_error("virtual template badness");
    }

    void ApplyMutation(const Mutation& mut);
    { }

    void ApplyMutations(const std::vector<Mutation>& muts)
    { }

private:
    Template& master_;
};

} // namespace Consensus
} // namespace PacBio
