/** A single checkable assertion (☐ line or table row with ☐). */
export interface CheckItem {
  id: string;
  text: string;
  status: 'pending' | 'pass' | 'fail' | 'skip';
  note: string;
  /** Filenames of attached screenshots (relative to report dir). */
  screenshots: string[];
}

/** A structured step within a test case (Do, Verify, Prerequisite, etc.). */
export interface TestStep {
  kind: 'do' | 'verify' | 'prerequisite' | 'expected' | 'why-manual' | 'note' | 'text';
  text: string;
  checks: CheckItem[];
}

/** A single test case (### heading). */
export interface TestCase {
  id: string;
  number: string;
  title: string;
  steps: TestStep[];
  /** Table-based checks (from markdown tables with ☐). */
  tableChecks: CheckItem[];
}

/** A top-level section (## heading). */
export interface TestSection {
  id: string;
  number: string;
  title: string;
  /** Context note from blockquotes. */
  contextNote: string;
  cases: TestCase[];
}

/** The full parsed test plan. */
export interface TestPlan {
  title: string;
  preamble: string;
  sections: TestSection[];
}

/** Serializable session state (what gets saved to localStorage and exported). */
export interface ReviewSession {
  sourceFile: string;
  startedAt: string;
  updatedAt: string;
  tester: string;
  items: Record<string, Pick<CheckItem, 'status' | 'note' | 'screenshots'>>;
}

/** Stats computed from a session. */
export interface ReviewStats {
  total: number;
  pass: number;
  fail: number;
  skip: number;
  pending: number;
}
