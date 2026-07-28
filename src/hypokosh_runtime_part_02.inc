    case GovernedEpistemicStatus::EvidenceRequired: return "evidence_required";
    case GovernedEpistemicStatus::Abstain: return "abstain";
    case GovernedEpistemicStatus::Speculative: return "speculative";
  }
  return "abstain";
}

}  // namespace graphene
